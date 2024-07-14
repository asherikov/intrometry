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
    class ComboPublisherFixture : public ::testing::Test, public intrometry_tests::SubscriberNode
    {
    public:
        intrometry::ComboPublisher<intrometry_tests::ArilesDebug> intrometry_publisher_;

    public:
        ComboPublisherFixture()
        {
            intrometry_tests::SubscriberNode::initialize("combopublisherfixture");
            intrometry_publisher_.initialize(
                    intrometry::Source::Parameters(/*persistent_structure=*/true), "ComboPublisherFixture");
        }
    };

    class MultiPublisherFixture : public ::testing::Test
    {
    public:
        using MultiPub = intrometry::ComboPublisher<intrometry_tests::ArilesDebug, intrometry_tests::ArilesDebug1>;
        using MultiPubVec = std::vector<MultiPub>;
    };
}  // namespace


TEST_F(ComboPublisherFixture, Simple)
{
    for (intrometry_publisher_.get<intrometry_tests::ArilesDebug>().size_ = 0;
         intrometry_publisher_.get<intrometry_tests::ArilesDebug>().size_ < 5;
         ++intrometry_publisher_.get<intrometry_tests::ArilesDebug>().size_)
    {
        intrometry_publisher_.write();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ASSERT_TRUE(checkReceived());
}

TEST_F(MultiPublisherFixture, Multi)
{
    MultiPubVec pub_vector;
    std::array<intrometry_tests::SubscriberNode, 10> sub_vector;

    {
        pub_vector.resize(10);
        std::size_t i = 0;
        for (MultiPub &pub : pub_vector)
        {
            std::stringstream strstream;
            strstream << "test" << i;
            pub.initialize(intrometry::Source::Parameters(/*persistent_structure=*/true), strstream.str());
            ++i;
        }

        i = 0;
        for (intrometry_tests::SubscriberNode &sub : sub_vector)
        {
            std::stringstream strstream;
            strstream << "test" << i;
            sub.initialize(strstream.str());
            ++i;
        }
    }


    for (std::size_t i = 0; i < 5; ++i)
    {
        for (MultiPub &pub : pub_vector)
        {
            pub.get<intrometry_tests::ArilesDebug>().size_ = i;
            pub.get<intrometry_tests::ArilesDebug1>().size_ = i * 10;
            pub.write();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }


    for (const intrometry_tests::SubscriberNode &node : sub_vector)
    {
        ASSERT_TRUE(node.checkReceived());
    }
}


int main(int argc, char **argv)
{
    rclcpp::init(argc, argv);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
