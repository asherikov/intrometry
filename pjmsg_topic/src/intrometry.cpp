/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#include <unordered_map>
#include <atomic>

#include <ariles2/visitors/namevalue2.h>
#include <thread_supervisor/supervisor.h>

#include <rclcpp/rclcpp.hpp>
#include <plotjuggler_msgs/msg/statistics_names.hpp>
#include <plotjuggler_msgs/msg/statistics_values.hpp>


#include "intrometry/intrometry.h"
#include "intrometry/backend/utils.h"
#include "intrometry/pjmsg_topic/sink.h"


namespace
{
    using NamesMsg = plotjuggler_msgs::msg::StatisticsNames;
    using ValuesMsg = plotjuggler_msgs::msg::StatisticsValues;

    using NamesPublisherPtr = rclcpp::Publisher<NamesMsg>::SharedPtr;
    using ValuesPublisherPtr = rclcpp::Publisher<ValuesMsg>::SharedPtr;

    struct Message
    {
        NamesMsg names_;
        ValuesMsg values_;
    };
}  // namespace


namespace
{
    class NameValueContainer : public ariles2::namevalue2::NameValueContainer
    {
    public:
        std::shared_ptr<Message> message_in_;
        std::shared_ptr<Message> message_out_;
        std::size_t previous_size_ = 0;
        bool new_names_version_ = false;

    public:
        NameValueContainer()
        {
            message_out_ = std::make_shared<Message>();
            message_in_ = std::make_shared<Message>();
        }

        bool swap()
        {
            const bool publish_names = new_names_version_;
            if (new_names_version_)
            {
                // message_out_->names_ = message_in_->names_;
                message_out_->names_.names.resize(message_in_->names_.names.size());
                message_out_->values_.values.resize(message_out_->names_.names.size());
                message_out_->names_.names_version = message_in_->names_.names_version;
                message_out_->values_.names_version = message_out_->names_.names_version;
                new_names_version_ = false;
            }
            std::swap(message_out_, message_in_);
            return publish_names;
        }

        void finalize(
                const bool persistent_structure,
                const rclcpp::Time &timestamp,
                std::atomic<uint32_t> &names_version)
        {
            message_in_->names_.header.stamp = timestamp;
            message_in_->values_.header.stamp = timestamp;

            // we cannot know for sure that the names have not changed
            // without comparing all the names, do our best
            if (not persistent_structure or previous_size_ != size())
            {
                message_in_->names_.names_version = names_version.fetch_add(1);
                message_in_->values_.names_version = message_in_->names_.names_version;
                new_names_version_ = true;
            }

            previous_size_ = size();
        }

        std::string &name(const std::size_t index)
        {
            return (message_in_->names_.names[index]); // NOLINT
        }
        double &value(const std::size_t index)
        {
            return (message_in_->values_.values[index]); // NOLINT
        }

        void reserve(const std::size_t size)
        {
            message_in_->names_.names.reserve(size);
            message_in_->values_.values.reserve(size);
        }
        [[nodiscard]] std::size_t size() const
        {
            return (message_in_->names_.names.size());
        }
        void resize(const std::size_t size)
        {
            message_in_->names_.names.resize(size);
            message_in_->values_.values.resize(size);
        }
    };
}  // namespace


namespace
{
    class WriterWrapper
    {
    public:
        const std::string id_;  // NOLINT
        ariles2::namevalue2::Writer::Parameters writer_parameters_;
        std::shared_ptr<NameValueContainer> data_;
        ariles2::namevalue2::Writer writer_;

        std::mutex mutex_in_;
        std::mutex mutex_out_;
        std::atomic<bool> flushed_;

    public:
        WriterWrapper(
                const ariles2::DefaultBase &source,
                std::string id,
                const bool persistent_structure,
                std::atomic<uint32_t> &names_version)
          : id_(std::move(id)), data_(std::make_shared<NameValueContainer>()), writer_(data_)
        {
            writer_parameters_ = writer_.getDefaultParameters();
            if (persistent_structure)
            {
                writer_parameters_.persistent_structure_ = true;
            }

            // write to allocate memory
            ariles2::apply(writer_, source, id_);
            data_->finalize(writer_parameters_.persistent_structure_, rclcpp::Time(0), names_version);

            flushed_ = true;  // do not publish on assignment
        }


        void publish(const NamesPublisherPtr &names_sink, const ValuesPublisherPtr &values_sink)
        {
            if (not flushed_)
            {
                if (mutex_in_.try_lock() && mutex_out_.try_lock())
                {
                    const bool publish_names = data_->swap();
                    mutex_in_.unlock();
                    if (publish_names)
                    {
                        names_sink->publish(data_->message_out_->names_);
                    }
                    values_sink->publish(data_->message_out_->values_);
                    flushed_ = true;
                    mutex_out_.unlock();
                }
            }
        }


        void write(
                const ariles2::DefaultBase &source,
                const rclcpp::Time &timestamp,
                std::atomic<uint32_t> &names_version)
        {
            if (mutex_in_.try_lock())
            {
                ariles2::apply(writer_, source, id_);
                data_->finalize(writer_parameters_.persistent_structure_, timestamp, names_version);
                flushed_ = false;
                mutex_in_.unlock();
            }
        }
    };
}  // namespace

namespace intrometry::pjmsg_topic::sink
{
    Parameters::Parameters(const std::string &id)
    {
        rate_ = 500;
        id_ = id;
    }

    Parameters::Parameters(const char *id)
    {
        rate_ = 500;
        id_ = id;
    }

    Parameters &Parameters::rate(const std::size_t value)
    {
        rate_ = value;
        return (*this);
    }

    Parameters &Parameters::id(const std::string &value)
    {
        id_ = value;
        return (*this);
    }
}  // namespace intrometry::pjmsg_topic::sink

namespace
{
    class ROSLogger
    {
    public:
        bool initialized_;
        std::shared_ptr<rclcpp::Node> node_;

    public:
        ROSLogger()
        {
            initialized_ = false;
        }

        template <class... t_Args>
        void log(t_Args &&...args) const
        {
            if (initialized_)
            {
                // RCLCPP_*_STREAM uses stringstream anyway
                std::stringstream strstream;  // NOLINT clang-tidy wants this const
                (strstream << ... << std::forward<t_Args>(args));
                RCLCPP_WARN(node_->get_logger(), "%s", strstream.str().c_str());
            }
        }

        void initializeLogger(const std::shared_ptr<rclcpp::Node> &node)
        {
            node_ = node;
            initialized_ = true;
        }
    };
}  // namespace


namespace intrometry::pjmsg_topic::sink
{
    class Implementation
    {
    public:
        std::shared_ptr<rclcpp::Node> node_;
        rclcpp::executors::SingleThreadedExecutor executor_;

    protected:
        NamesPublisherPtr names_publisher_;
        ValuesPublisherPtr values_publisher_;

    public:
        std::atomic<uint32_t> names_version_;

        intrometry::backend::SourceContainer<WriterWrapper> sources_;
        tut::thread::Supervisor<ROSLogger> thread_supervisor_;

    public:
        Implementation(const std::string &sink_id, const std::size_t rate)
        {
            names_version_ = intrometry::backend::getRandomUInt32();

            const std::string node_id = intrometry::backend::normalizeId(sink_id);
            const std::string random_id = intrometry::backend::getRandomId(8);
            const std::string topic_prefix =
                    intrometry::backend::str_concat("intrometry/", node_id.empty() ? random_id : node_id);

            node_ = std::make_shared<rclcpp::Node>(
                    intrometry::backend::str_concat("intrometry_", node_id, "_", random_id),
                    // try to be stealthy and use minimal resources
                    rclcpp::NodeOptions()
                            .enable_topic_statistics(false)
                            .start_parameter_services(false)
                            .start_parameter_event_publisher(false)
                            .append_parameter_override("use_sim_time", false)
                            .use_clock_thread(false)
                            .enable_rosout(false));
            thread_supervisor_.initializeLogger(node_);
            executor_.add_node(node_);

            names_publisher_ = node_->create_publisher<NamesMsg>(
                    intrometry::backend::str_concat(topic_prefix, "/names"),
                    rclcpp::QoS(/*history_depth=*/20).reliable().transient_local());
            values_publisher_ = node_->create_publisher<ValuesMsg>(
                    intrometry::backend::str_concat(topic_prefix, "/values"),
                    rclcpp::QoS(/*history_depth=*/20).best_effort().durability_volatile());


            thread_supervisor_.add(
                    tut::thread::Parameters(
                            tut::thread::Parameters::Restart(/*attempts=*/100, /*sleep_ms=*/50),
                            tut::thread::Parameters::TerminationPolicy::IGNORE,
                            tut::thread::Parameters::ExceptionPolicy::CATCH),
                    &Implementation::spin,
                    this,
                    rate);
        }

        virtual ~Implementation()
        {
            thread_supervisor_.stop();
        }


        void spin(const std::size_t rate)
        {
            intrometry::backend::RateTimer timer(rate);

            if (timer.valid())
            {
                timer.start();

                while (rclcpp::ok() and not thread_supervisor_.isInterrupted())
                {
                    flush();
                    executor_.spin_some();

                    timer.step();
                }
                flush();
            }
            else
            {
                thread_supervisor_.log("Incorrect spin rate");
            }
            thread_supervisor_.interrupt();
        }


        void flush()
        {
            sources_.tryFlush([this](WriterWrapper &writer)
                              { writer.publish(names_publisher_, values_publisher_); });
        }
    };
}  // namespace intrometry::pjmsg_topic::sink



namespace intrometry::pjmsg_topic
{
    Sink::~Sink() = default;


    bool Sink::initialize()
    {
        if (not rclcpp::ok() or parameters_.id_.empty())
        {
            return (false);
        }
        make_pimpl(parameters_.id_, parameters_.rate_);
        return (true);
    }


    void Sink::assign(const std::string &id, const ariles2::DefaultBase &source, const Source::Parameters &parameters)
    {
        if (pimpl_)
        {
            pimpl_->sources_.tryEmplace(id, source, parameters.persistent_structure_, pimpl_->names_version_);
        }
    }


    void Sink::retract(const std::string &id, const ariles2::DefaultBase &source)
    {
        if (pimpl_)
        {
            pimpl_->sources_.erase(id, source);
        }
    }


    void Sink::write(const std::string &id, const ariles2::DefaultBase &source, const uint64_t timestamp)
    {
        if (pimpl_)
        {
            if (not pimpl_->sources_.tryWrite(
                        id,
                        source,
                        [this, &source, &timestamp](WriterWrapper &writer)
                        {
                            writer.write(
                                    source,
                                    (0 == timestamp) ? pimpl_->node_->now() :
                                                       rclcpp::Time(static_cast<rcl_time_point_value_t>(timestamp)),
                                    pimpl_->names_version_);
                        }))
            {
                pimpl_->thread_supervisor_.log(
                        "Measurement source handler is not assigned, skipping id: ", source.arilesDefaultID());
            }
        }
    }


    void Sink::flush()
    {
        if (pimpl_)
        {
            pimpl_->flush();
        }
    }
}  // namespace intrometry::pjmsg_topic
