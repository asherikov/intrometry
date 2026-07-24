/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#include "pjmsg_topic_common.h"


namespace
{
    class PjmsgTopicIntrometryFixture : public ::testing::Test, public intrometry_tests::SubscriberNode
    {
    public:
        intrometry::pjmsg_topic::Sink intrometry_sink_;

    public:
        PjmsgTopicIntrometryFixture() : intrometry_sink_("IntrometryFixture")
        {
            intrometry_tests::SubscriberNode::initialize("intrometryfixture");
            intrometry_sink_.initialize();
        }
    };
}  // namespace


TEST_F(PjmsgTopicIntrometryFixture, ArilesDynamic)
{
    intrometry_tests::ArilesDebug debug;
    intrometry_sink_.assign(debug);

    for (debug.size_ = 0; debug.size_ < 5; ++debug.size_)
    {
        intrometry_sink_.write(debug);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    intrometry_sink_.retract(debug);
    ASSERT_TRUE(checkReceived());
}


TEST_F(PjmsgTopicIntrometryFixture, ArilesPersistent)
{
    intrometry_tests::ArilesDebug debug{};
    intrometry_sink_.assign(debug, intrometry::Source::Parameters(/*persistent_structure=*/true));
    debug.vec_ = { 3.4, 2.2, 2.1 };

    for (std::size_t i = 0; i < 3; ++i)
    {
        intrometry_sink_.write(debug);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    intrometry_sink_.retract(debug);
    ASSERT_TRUE(checkReceived());
}


TEST_F(PjmsgTopicIntrometryFixture, MultipleSources)
{
    const intrometry_tests::ArilesDebug debug0{};
    const intrometry_tests::ArilesDebug1 debug1{};
    intrometry_sink_.assignBatch(intrometry::Source::Parameters(/*persistent_structure=*/true), debug0, debug1);

    for (std::size_t i = 0; i < 3; ++i)
    {
        intrometry_sink_.writeBatch(0, debug0, debug1);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    intrometry_sink_.retractBatch(debug0, debug1);
    ASSERT_TRUE(checkReceived());
}


TEST_F(PjmsgTopicIntrometryFixture, Flush)
{
    const intrometry_tests::ArilesDebug debug{};
    intrometry_sink_.assign(debug, intrometry::Source::Parameters(/*persistent_structure=*/true));

    intrometry_sink_.write(debug);
    intrometry_sink_.flush();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    intrometry_sink_.retract(debug);
    ASSERT_TRUE(checkReceived());
}


TEST_F(PjmsgTopicIntrometryFixture, ContainerDynamic)
{
    intrometry_tests::ArilesDebugContainer container{};
    intrometry_sink_.assign(container);

    for (std::size_t i = 0; i < 5; ++i)
    {
        container.entries_.resize(i + 1);
        for (std::size_t j = 0; j < container.entries_.size(); ++j)
        {
            container.entries_.at(j).size_ = j;
            container.entries_.at(j).duration_ = static_cast<double>(i);
        }
        intrometry_sink_.write(container);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    intrometry_sink_.retract(container);
    ASSERT_TRUE(checkReceived());
}


TEST_F(PjmsgTopicIntrometryFixture, ContainerPersistent)
{
    intrometry_tests::ArilesDebugContainer container{};
    intrometry_sink_.assign(container, intrometry::Source::Parameters(/*persistent_structure=*/true));

    for (std::size_t i = 0; i < 5; ++i)
    {
        container.entries_.resize(i + 1);
        for (std::size_t j = 0; j < container.entries_.size(); ++j)
        {
            container.entries_.at(j).size_ = j;
            container.entries_.at(j).duration_ = static_cast<double>(i);
        }
        intrometry_sink_.write(container);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    intrometry_sink_.retract(container);
    ASSERT_TRUE(checkReceived());
}


TEST_F(PjmsgTopicIntrometryFixture, ContainerShrinking)
{
    intrometry_tests::ArilesDebugContainer container{};
    intrometry_sink_.assign(container, intrometry::Source::Parameters(/*persistent_structure=*/true));

    constexpr std::size_t start_size = 5;
    for (std::size_t i = 0; i < start_size; ++i)
    {
        container.entries_.resize(start_size - i);
        for (std::size_t j = 0; j < container.entries_.size(); ++j)
        {
            container.entries_.at(j).size_ = j;
            container.entries_.at(j).duration_ = static_cast<double>(i);
        }
        intrometry_sink_.write(container);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    intrometry_sink_.retract(container);
    ASSERT_TRUE(checkReceived());
}



int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
