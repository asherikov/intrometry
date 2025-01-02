/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/


#include "intrometry/api/intrometry.h"
#include "intrometry/pjmsg_topic/sink.h"

#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include <rclcpp/rclcpp.hpp>
#include <plotjuggler_msgs/msg/statistics_names.hpp>
#include <plotjuggler_msgs/msg/statistics_values.hpp>

#include <thread_supervisor/supervisor.h>
#include <ariles2/adapters/std_vector.h>


namespace intrometry_tests
{
    using NamesMsg = plotjuggler_msgs::msg::StatisticsNames;
    using ValuesMsg = plotjuggler_msgs::msg::StatisticsValues;

    using NamesSubscriptionPtr = rclcpp::Subscription<NamesMsg>::SharedPtr;
    using ValuesSubscriptionPtr = rclcpp::Subscription<ValuesMsg>::SharedPtr;


    class ArilesDebug : public ariles2::DefaultBase
    {
#define ARILES2_DEFAULT_ID "ArilesDebug"
#define ARILES2_ENTRIES(v)                                                                                             \
    ARILES2_TYPED_ENTRY_(v, duration, double)                                                                          \
    ARILES2_TYPED_ENTRY_(v, size, std::size_t)                                                                         \
    ARILES2_TYPED_ENTRY_(v, vec, std::vector<float>)
#include ARILES2_INITIALIZE
    public:
        virtual ~ArilesDebug() = default;
    };

    class ArilesDebug1 : public ArilesDebug
    {
#define ARILES2_DEFAULT_ID "ArilesDebug1"
#define ARILES2_ENTRIES(v) ARILES2_PARENT(v, ArilesDebug)
#include ARILES2_INITIALIZE
    public:
        virtual ~ArilesDebug1() = default;
    };


    template <class t_Message>
    class Subscription
    {
    public:
        typename rclcpp::Subscription<t_Message>::SharedPtr subscription_;
        std::atomic<std::size_t> counter_ = 0;

    public:
        void subscribe(std::shared_ptr<rclcpp::Node> &node, const std::string &topic)
        {
            subscription_ = node->create_subscription<t_Message>(
                    topic,
                    rclcpp::QoS(/*history_depth=*/10).best_effort(),
                    [this](const t_Message &msg)
                    {
                        std::stringstream output;
                        plotjuggler_msgs::msg::to_block_style_yaml(msg, output);
                        std::cerr << output.str();
                        ++counter_;
                    });
        }

        [[nodiscard]] bool atLeastOne() const
        {
            return (counter_ > 0);
        }
    };


    class SubscriberNode : public tut::thread::InheritableSupervisor<>
    {
    public:
        std::shared_ptr<rclcpp::Node> node_;
        Subscription<NamesMsg> names_subscription_;
        Subscription<ValuesMsg> values_subscription_;
        std::thread spinner_;

    protected:
        // cppcheck-suppress virtualCallInConstructor
        void stopSupervisedThreads() override
        {
            getThreadSupervisor().stop();
        }

    public:
        void initialize(const std::string &topic)
        {
            node_ = std::make_shared<rclcpp::Node>(topic + "_intrometry_tests");

            names_subscription_.subscribe(node_, std::string("/intrometry/") + topic + "/names");
            values_subscription_.subscribe(node_, std::string("/intrometry/") + topic + "/values");

            this->addSupervisedThread(
                    tut::thread::Parameters(
                            tut::thread::Parameters::Restart(/*attempts=*/1, /*sleep_ms=*/0),
                            tut::thread::Parameters::TerminationPolicy::KILLALL,
                            tut::thread::Parameters::ExceptionPolicy::PASS),
                    &SubscriberNode::spin,
                    this);
        }


        [[nodiscard]] bool checkReceived() const
        {
            return (names_subscription_.atLeastOne() and values_subscription_.atLeastOne());
        }


        virtual ~SubscriberNode()
        {
            stopSupervisedThreads();
        }


        void spin()
        {
            const std::chrono::nanoseconds step(std::nano::den / 1000);

            std::chrono::time_point<std::chrono::steady_clock> time_threshold = std::chrono::steady_clock::now();
            while (rclcpp::ok() and not isThreadSupervisorInterrupted())
            {
                rclcpp::spin_some(node_);
                time_threshold += step;
                std::this_thread::sleep_until(time_threshold);
            }
            getThreadSupervisor().interrupt();
        }
    };
}  // namespace intrometry_tests