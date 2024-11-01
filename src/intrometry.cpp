/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#include <random>
#include <unordered_map>
#include <ratio>

#include <ariles2/visitors/namevalue2.h>
#include <thread_supervisor/supervisor.h>

#include <rclcpp/rclcpp.hpp>
#include <plotjuggler_msgs/msg/statistics_names.hpp>
#include <plotjuggler_msgs/msg/statistics_values.hpp>


#include "intrometry/intrometry.h"


namespace
{
    using NamesMsg = plotjuggler_msgs::msg::StatisticsNames;
    using ValuesMsg = plotjuggler_msgs::msg::StatisticsValues;

    using NamesPublisherPtr = rclcpp::Publisher<NamesMsg>::SharedPtr;
    using ValuesPublisherPtr = rclcpp::Publisher<ValuesMsg>::SharedPtr;


    template <typename... t_String>
    std::string str_concat(t_String &&...strings)
    {
        std::string result;
        (result += ... += strings);
        return result;
    }
}  // namespace


namespace intrometry
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
}  // namespace intrometry


namespace intrometry
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


        void publish(const NamesPublisherPtr &names_publisher, const ValuesPublisherPtr &values_publisher)
        {
            if (mutex_.try_lock())
            {
                if (not published_)
                {
                    if (data_->new_names_version_)
                    {
                        names_publisher->publish(data_->names_);
                        data_->new_names_version_ = false;
                    }
                    values_publisher->publish(data_->values_);
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
}  // namespace intrometry


namespace intrometry
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


    class Publisher::Implementation
    {
    protected:
        using SourceSet = std::unordered_map<std::string, WriterWrapper>;


    protected:
        std::shared_ptr<rclcpp::Node> node_;
        tut::thread::Supervisor<ROSLogger> thread_supervisor_;
        uint32_t names_version_;

        SourceSet sources_;
        std::mutex update_mutex_;
        std::mutex publish_mutex_;

        NamesPublisherPtr names_publisher_;
        ValuesPublisherPtr values_publisher_;

    public:
        Implementation(const std::string &publisher_id, const std::size_t rate)
        {
            std::mt19937 gen((std::random_device())());

            std::uniform_int_distribution<uint32_t> distrib(
                    std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());
            names_version_ = distrib(gen);

            std::string valid_chars = "0123456789abcdefghijklmnopqrstuvwxyz";


            std::string node_id;
            node_id.resize(publisher_id.size());
            std::transform(
                    publisher_id.cbegin(),
                    publisher_id.cend(),
                    node_id.begin(),
                    [valid_chars](unsigned char c) -> unsigned char
                    {
                        c = std::tolower(c);
                        if (valid_chars.find(static_cast<char>(c)) == std::string::npos)
                        {
                            return ('_');
                        }
                        return (c);
                    });

            const std::size_t first_non_underscore = node_id.find_first_not_of('_');
            if (first_non_underscore == std::string::npos)
            {
                node_id = "";
            }
            else
            {
                if (first_non_underscore > 0)
                {
                    node_id = node_id.substr(first_non_underscore);
                }
            }

            std::shuffle(valid_chars.begin(), valid_chars.end(), gen);
            const std::string random_id = valid_chars.substr(0, 8);
            const std::string topic_prefix = str_concat("intrometry/", node_id.empty() ? random_id : node_id);

            node_ = std::make_shared<rclcpp::Node>(
                    str_concat("intrometry_", node_id, "_", random_id),
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
                    str_concat(topic_prefix, "/names"), rclcpp::QoS(/*history_depth=*/20).reliable().transient_local());
            values_publisher_ = node_->create_publisher<ValuesMsg>(
                    str_concat(topic_prefix, "/values"),
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
            const std::chrono::nanoseconds step(std::nano::den / rate);

            if (step.count() > 0)
            {
                std::chrono::time_point<std::chrono::steady_clock> time_threshold = std::chrono::steady_clock::now();

                while (rclcpp::ok() and not thread_supervisor_.isInterrupted())
                {
                    if (publish_mutex_.try_lock())
                    {
                        for (std::pair<const std::string, WriterWrapper> &source : sources_)
                        {
                            source.second.publish(names_publisher_, values_publisher_);
                        }

                        publish_mutex_.unlock();
                    }
                    rclcpp::spin_some(node_);

                    time_threshold += step;
                    std::this_thread::sleep_until(time_threshold);
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
            const std::lock_guard<std::mutex> update_lock(update_mutex_);
            const std::lock_guard<std::mutex> publish_lock(publish_mutex_);

            sources_.try_emplace(source.arilesDefaultID(), source, parameters.persistent_structure_, names_version_);
        }

        void retract(const ariles2::DefaultBase &source)
        {
            const std::lock_guard<std::mutex> update_lock(update_mutex_);
            const std::lock_guard<std::mutex> publish_lock(publish_mutex_);

            sources_.erase(source.arilesDefaultID());
        }

        void write(const ariles2::DefaultBase &source, const rclcpp::Time &timestamp)
        {
            if (update_mutex_.try_lock())
            {
                const SourceSet::iterator source_it = sources_.find(source.arilesDefaultID());

                if (sources_.end() == source_it)
                {
                    thread_supervisor_.log(
                            "Measurement source handler is not assigned, skipping id: ", source.arilesDefaultID());
                }
                else
                {
                    if (0 == timestamp.nanoseconds())
                    {
                        source_it->second.write(source, node_->now(), names_version_);
                    }
                    else
                    {
                        source_it->second.write(source, timestamp, names_version_);
                    }
                }

                update_mutex_.unlock();
            }
        }
    };
}  // namespace intrometry



namespace intrometry
{
    Publisher::Publisher() = default;
    Publisher::~Publisher() = default;


    bool Publisher::initialize(const std::string &publisher_id, const Parameters &parameters)
    {
        if (not rclcpp::ok() or publisher_id.empty())
        {
            return (false);
        }
        pimpl_ = std::make_shared<Publisher::Implementation>(publisher_id, parameters.rate_);
        return (true);
    }


    void Publisher::assign(const ariles2::DefaultBase &source, const Source::Parameters &parameters)
    {
        if (pimpl_)
        {
            pimpl_->assign(source, parameters);
        }
    }


    void Publisher::retract(const ariles2::DefaultBase &source)
    {
        if (pimpl_)
        {
            pimpl_->retract(source);
        }
    }


    void Publisher::write(const ariles2::DefaultBase &source, const uint64_t timestamp)
    {
        if (pimpl_)
        {
            pimpl_->write(source, rclcpp::Time(static_cast<rcl_time_point_value_t>(timestamp)));
        }
    }
}  // namespace intrometry
