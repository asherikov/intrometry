/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#include "common.h"


namespace
{
    class IntrometryFixture : public ::testing::Test, public intrometry_tests::SubscriberNode
    {
    public:
        intrometry::Publisher intrometry_publisher_;

    public:
        IntrometryFixture()
        {
            intrometry_tests::SubscriberNode::initialize("intrometryfixture");
            intrometry_publisher_.initialize("IntrometryFixture");
        }
    };
}  // namespace


TEST_F(IntrometryFixture, ArilesDynamic)
{
    intrometry_tests::ArilesDebug debug;
    intrometry_publisher_.assign(debug);

    for (debug.size_ = 0; debug.size_ < 5; ++debug.size_)
    {
        intrometry_publisher_.write(debug);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    intrometry_publisher_.retract(debug);
    ASSERT_TRUE(checkReceived());
}


TEST_F(IntrometryFixture, ArilesPersistent)
{
    intrometry_tests::ArilesDebug debug{};
    intrometry_publisher_.assign(debug, intrometry::Source::Parameters(/*persistent_structure=*/true));
    debug.vec_ = { 3.4, 2.2, 2.1 };

    for (std::size_t i = 0; i < 3; ++i)
    {
        intrometry_publisher_.write(debug);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    intrometry_publisher_.retract(debug);
    ASSERT_TRUE(checkReceived());
}


TEST_F(IntrometryFixture, MultipleSources)
{
    const intrometry_tests::ArilesDebug debug0{};
    const intrometry_tests::ArilesDebug1 debug1{};
    intrometry_publisher_.assignBatch(intrometry::Source::Parameters(/*persistent_structure=*/true), debug0, debug1);

    for (std::size_t i = 0; i < 3; ++i)
    {
        intrometry_publisher_.writeBatch(0, debug0, debug1);
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    intrometry_publisher_.retractBatch(debug0, debug1);
    ASSERT_TRUE(checkReceived());
}



int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
