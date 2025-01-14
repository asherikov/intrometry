/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#include <unordered_map>

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
}  // namespace


namespace
{
    class NameValueContainer : public ariles2::namevalue2::NameValueContainer
    {
    public:
        NamesMsg names_;
        ValuesMsg values_;
        std::size_t previous_size_ = 0;
        bool new_names_version_ = false;

    public:  // ariles stuff
        void finalize(const bool persistent_structure, const rclcpp::Time &timestamp, uint32_t &names_version)
        {
            names_.header.stamp = timestamp;
            values_.header.stamp = timestamp;

            // we cannot know for sure that the names have not changed
            // without comparing all the names, do our best
            if (not persistent_structure or previous_size_ != size())
            {
                names_.names_version = names_version;
                values_.names_version = names_version;
                new_names_version_ = true;
                ++names_version;
            }

            previous_size_ = size();
        }

        std::string &name(const std::size_t index)
        {
            return (names_.names[index]);
        }
        double &value(const std::size_t index)
        {
            return (values_.values[index]);
        }

        void reserve(const std::size_t size)
        {
            names_.names.reserve(size);
            values_.values.reserve(size);
        }
        [[nodiscard]] std::size_t size() const
        {
            return (names_.names.size());
        }
        void resize(const std::size_t size)
        {
            names_.names.resize(size);
            values_.values.resize(size);
        }
    };
}  // namespace


namespace
{
    class WriterWrapper
    {
    public:
        std::mutex mutex_;
        ariles2::namevalue2::Writer::Parameters writer_parameters_;
        std::shared_ptr<NameValueContainer> data_;
        ariles2::namevalue2::Writer writer_;

        bool published_;


    public:
        WriterWrapper(const ariles2::DefaultBase &source, const bool persistent_structure, uint32_t &names_version)
          : data_(std::make_shared<NameValueContainer>()), writer_(data_)
        {
            writer_parameters_ = writer_.getDefaultParameters();
            if (persistent_structure)
            {
                writer_parameters_.persistent_structure_ = true;
            }

            // write to allocate memory
            ariles2::apply(writer_, source);
            data_->finalize(writer_parameters_.persistent_structure_, rclcpp::Time(0), names_version);

            published_ = true;  // do not publish on assignment
        }


        void publish(const NamesPublisherPtr &names_sink, const ValuesPublisherPtr &values_sink)
        {
            if (mutex_.try_lock())
            {
                if (not published_)
                {
                    if (data_->new_names_version_)
                    {
                        names_sink->publish(data_->names_);
                        data_->new_names_version_ = false;
                    }
                    values_sink->publish(data_->values_);
                    published_ = true;
                }
                mutex_.unlock();
            }
        }


        void write(const ariles2::DefaultBase &source, const rclcpp::Time &timestamp, uint32_t &names_version)
        {
            if (mutex_.try_lock())
            {
                ariles2::apply(writer_, source);
                data_->finalize(writer_parameters_.persistent_structure_, timestamp, names_version);
                published_ = false;

                mutex_.unlock();
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

namespace intrometry::pjmsg_topic
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
                (strstream << ... << args);
                RCLCPP_WARN(node_->get_logger(), "%s", strstream.str().c_str());
            }
        }

        void initializeLogger(const std::shared_ptr<rclcpp::Node> &node)
        {
            node_ = node;
            initialized_ = true;
        }
    };


    namespace sink
    {
        class Implementation
        {
        protected:
            std::shared_ptr<rclcpp::Node> node_;
            tut::thread::Supervisor<ROSLogger> thread_supervisor_;
            uint32_t names_version_;

            intrometry::backend::SourceContainer<WriterWrapper> sources_;

            NamesPublisherPtr names_publisher_;
            ValuesPublisherPtr values_publisher_;

        public:
            Implementation(const std::string &sink_id, const std::size_t rate)
            {
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
                        sources_.tryVisit([this](WriterWrapper &writer)
                                          { writer.publish(names_publisher_, values_publisher_); });
                        rclcpp::spin_some(node_);

                        timer.step();
                    }
                }
                else
                {
                    thread_supervisor_.log("Incorrect spin rate");
                }
                thread_supervisor_.interrupt();
            }


            void assign(const ariles2::DefaultBase &source, const Source::Parameters &parameters)
            {
                sources_.tryEmplace(source, parameters.persistent_structure_, names_version_);
            }

            void retract(const ariles2::DefaultBase &source)
            {
                sources_.erase(source);
            }

            void write(const ariles2::DefaultBase &source, const rclcpp::Time &timestamp)
            {
                if (not sources_.tryVisit(
                            source,
                            [this, &source, &timestamp](WriterWrapper &writer) {
                                writer.write(
                                        source,
                                        (0 == timestamp.nanoseconds()) ? node_->now() : timestamp,
                                        names_version_);
                            }))
                {
                    thread_supervisor_.log(
                            "Measurement source handler is not assigned, skipping id: ", source.arilesDefaultID());
                }
            }
        };
    }  // namespace sink
}  // namespace intrometry::pjmsg_topic



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


    void Sink::assign(const ariles2::DefaultBase &source, const Source::Parameters &parameters)
    {
        if (pimpl_)
        {
            pimpl_->assign(source, parameters);
        }
    }


    void Sink::retract(const ariles2::DefaultBase &source)
    {
        if (pimpl_)
        {
            pimpl_->retract(source);
        }
    }


    void Sink::write(const ariles2::DefaultBase &source, const uint64_t timestamp)
    {
        if (pimpl_)
        {
            pimpl_->write(source, rclcpp::Time(static_cast<rcl_time_point_value_t>(timestamp)));
        }
    }
}  // namespace intrometry::pjmsg_topic
