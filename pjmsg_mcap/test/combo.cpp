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
    class ComboSinkFixture : public ::testing::Test
    {
    public:
        intrometry::ComboSink<intrometry_tests::ArilesDebug> intrometry_sink_;

    public:
        ComboSinkFixture()
        {
            intrometry_sink_.initialize<intrometry::pjmsg_mcap::Sink>(
                    intrometry::Source::Parameters(/*persistent_structure=*/true), "ComboSinkFixture");
        }
    };

    class MultiSinkFixture : public ::testing::Test
    {
    public:
        using MultiPub = intrometry::ComboSink<intrometry_tests::ArilesDebug, intrometry_tests::ArilesDebug1>;
        using MultiPubVec = std::vector<MultiPub>;
    };
}  // namespace


TEST_F(ComboSinkFixture, Simple)
{
    for (intrometry_sink_.get<intrometry_tests::ArilesDebug>().size_ = 0;
         intrometry_sink_.get<intrometry_tests::ArilesDebug>().size_ < 5;
         ++intrometry_sink_.get<intrometry_tests::ArilesDebug>().size_)
    {
        intrometry_sink_.write();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    ASSERT_TRUE(true);
}

TEST_F(MultiSinkFixture, Multi)
{
    MultiPubVec pub_vector;

    {
        pub_vector.resize(10);
        std::size_t i = 0;
        for (MultiPub &pub : pub_vector)
        {
            std::stringstream strstream;
            strstream << "test" << i;
            pub.initialize<intrometry::pjmsg_mcap::Sink>(
                    intrometry::Source::Parameters(/*persistent_structure=*/true), strstream.str());
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

    ASSERT_TRUE(true);
}


int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
