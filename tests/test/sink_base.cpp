/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#include <intrometry/pjmsg_mcap/all.h>
#include <intrometry/pjmsg_topic/all.h>
#include "common.h"


namespace
{
    class SinkBase : public ::testing::Test
    {
    public:
        std::shared_ptr<intrometry::pjmsg_mcap::Sink> sink_mcap_;
        std::shared_ptr<intrometry::pjmsg_topic::Sink> sink_topic_;
        std::shared_ptr<intrometry::Sink> sink_;

    public:
        SinkBase()
        {
            sink_mcap_ = std::make_shared<intrometry::pjmsg_mcap::Sink>("SinkBase");
            sink_topic_ = std::make_shared<intrometry::pjmsg_topic::Sink>("SinkBase");

            sink_mcap_->initialize();
            sink_topic_->initialize();
        }

        void useSink()
        {
            intrometry_tests::ArilesDebug debug;
            sink_->assign(debug);

            for (debug.size_ = 0; debug.size_ < 5; ++debug.size_)
            {
                sink_->write(debug);
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            sink_->retract(debug);
        }
    };
}  // namespace


TEST_F(SinkBase, MCAP)
{
    sink_ = sink_mcap_;
    useSink();
    ASSERT_TRUE(true);
}

TEST_F(SinkBase, Topic)
{
    sink_ = sink_topic_;
    useSink();
    ASSERT_TRUE(true);
}

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
