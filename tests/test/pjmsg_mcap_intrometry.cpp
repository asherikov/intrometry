/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#include "pjmsg_mcap_common.h"

#include <memory>


namespace
{
    class PjmsgMcapRaw
    {
    public:
        std::filesystem::path directory_;
        std::unique_ptr<intrometry::pjmsg_mcap::Sink> intrometry_sink_;
        static constexpr const char *sink_id_ = "intrometryfixtureraw";

    public:
        PjmsgMcapRaw()
          : directory_(std::filesystem::temp_directory_path() / "intrometry_mcap_raw")
        {
            std::filesystem::remove_all(directory_);
            intrometry_sink_ = std::make_unique<intrometry::pjmsg_mcap::Sink>(
                    intrometry::pjmsg_mcap::sink::Parameters("IntrometryFixtureRaw").directory(directory_));
            intrometry_sink_->initialize();
        }

        ~PjmsgMcapRaw()
        {
            std::filesystem::remove_all(directory_);
        }
    };

    class PjmsgMcapCompressed
    {
    public:
        std::filesystem::path directory_;
        std::unique_ptr<intrometry::pjmsg_mcap::Sink> intrometry_sink_;
        static constexpr const char *sink_id_ = "intrometryfixturecompressed";

    public:
        PjmsgMcapCompressed()
          : directory_(std::filesystem::temp_directory_path() / "intrometry_mcap_compressed")
        {
            std::filesystem::remove_all(directory_);
            intrometry_sink_ = std::make_unique<intrometry::pjmsg_mcap::Sink>(
                    intrometry::pjmsg_mcap::sink::Parameters("IntrometryFixtureCompressed")
                            .directory(directory_)
                            .compression(intrometry::pjmsg_mcap::sink::Parameters::Compression::ZSTD));
            intrometry_sink_->initialize();
        }

        ~PjmsgMcapCompressed()
        {
            std::filesystem::remove_all(directory_);
        }
    };

    template <class t_Base>
    class PjmsgMcapIntrometryFixture : public ::testing::Test, public t_Base
    {
    public:
        using t_Base::t_Base;
    };

    using PjmsgMcapIntrometryFixtureTypes = ::testing::Types<PjmsgMcapRaw, PjmsgMcapCompressed>;
    class NameGenerator
    {
    public:
        template <typename T>
        static std::string GetName(int /*unused*/)
        {
            if constexpr (std::is_same_v<T, PjmsgMcapRaw>)
            {
                return "PjmsgMcapRaw";
            }
            if constexpr (std::is_same_v<T, PjmsgMcapCompressed>)
            {
                return "PjmsgMcapCompressed";
            }
        }
    };
    TYPED_TEST_SUITE(PjmsgMcapIntrometryFixture, PjmsgMcapIntrometryFixtureTypes, NameGenerator);
}  // namespace


TYPED_TEST(PjmsgMcapIntrometryFixture, ArilesDynamic)
{
    intrometry_tests::ArilesDebug debug;
    this->intrometry_sink_->assign(debug);

    for (debug.size_ = 0; debug.size_ < 5; ++debug.size_)
    {
        this->intrometry_sink_->write(debug);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    this->intrometry_sink_->retract(debug);
    this->intrometry_sink_ = nullptr;
    ASSERT_NO_THROW(ASSERT_TRUE(intrometry_tests::checkMcap(this->directory_, this->sink_id_)));
}


TYPED_TEST(PjmsgMcapIntrometryFixture, ArilesPersistent)
{
    intrometry_tests::ArilesDebug debug{};
    this->intrometry_sink_->assign(debug, intrometry::Source::Parameters(/*persistent_structure=*/true));
    debug.vec_ = { 3.4, 2.2, 2.1 };

    for (std::size_t i = 0; i < 3; ++i)
    {
        this->intrometry_sink_->write(debug);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    this->intrometry_sink_->retract(debug);
    this->intrometry_sink_ = nullptr;
    ASSERT_NO_THROW(ASSERT_TRUE(intrometry_tests::checkMcap(this->directory_, this->sink_id_)));
}


TYPED_TEST(PjmsgMcapIntrometryFixture, MultipleSources)
{
    const intrometry_tests::ArilesDebug debug0{};
    const intrometry_tests::ArilesDebug1 debug1{};
    this->intrometry_sink_->assignBatch(intrometry::Source::Parameters(/*persistent_structure=*/true), debug0, debug1);

    for (std::size_t i = 0; i < 3; ++i)
    {
        this->intrometry_sink_->writeBatch(0, debug0, debug1);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    this->intrometry_sink_->retractBatch(debug0, debug1);
    this->intrometry_sink_ = nullptr;
    ASSERT_NO_THROW(ASSERT_TRUE(intrometry_tests::checkMcap(this->directory_, this->sink_id_)));
}


TYPED_TEST(PjmsgMcapIntrometryFixture, ContainerDynamic)
{
    intrometry_tests::ArilesDebugContainer container{};
    this->intrometry_sink_->assign(container);

    for (std::size_t i = 0; i < 5; ++i)
    {
        container.entries_.resize(i + 1);
        for (std::size_t j = 0; j < container.entries_.size(); ++j)
        {
            container.entries_.at(j).size_ = j;
            container.entries_.at(j).duration_ = static_cast<double>(i);
        }
        this->intrometry_sink_->write(container);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    this->intrometry_sink_->retract(container);
    this->intrometry_sink_ = nullptr;
    ASSERT_NO_THROW(ASSERT_TRUE(intrometry_tests::checkMcap(this->directory_, this->sink_id_)));
}


TYPED_TEST(PjmsgMcapIntrometryFixture, ContainerPersistent)
{
    intrometry_tests::ArilesDebugContainer container{};
    this->intrometry_sink_->assign(container, intrometry::Source::Parameters(/*persistent_structure=*/true));

    for (std::size_t i = 0; i < 5; ++i)
    {
        container.entries_.resize(i + 1);
        for (std::size_t j = 0; j < container.entries_.size(); ++j)
        {
            container.entries_.at(j).size_ = j;
            container.entries_.at(j).duration_ = static_cast<double>(i);
        }
        this->intrometry_sink_->write(container);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    this->intrometry_sink_->retract(container);
    this->intrometry_sink_ = nullptr;
    ASSERT_NO_THROW(ASSERT_TRUE(intrometry_tests::checkMcap(this->directory_, this->sink_id_)));
}


TYPED_TEST(PjmsgMcapIntrometryFixture, ContainerShrinking)
{
    intrometry_tests::ArilesDebugContainer container{};
    this->intrometry_sink_->assign(container, intrometry::Source::Parameters(/*persistent_structure=*/true));

    constexpr std::size_t start_size = 5;
    for (std::size_t i = 0; i < start_size; ++i)
    {
        container.entries_.resize(start_size - i);
        for (std::size_t j = 0; j < container.entries_.size(); ++j)
        {
            container.entries_.at(j).size_ = j;
            container.entries_.at(j).duration_ = static_cast<double>(i);
        }
        this->intrometry_sink_->write(container);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    this->intrometry_sink_->retract(container);
    this->intrometry_sink_ = nullptr;
    ASSERT_NO_THROW(ASSERT_TRUE(intrometry_tests::checkMcap(this->directory_, this->sink_id_)));
}


TYPED_TEST(PjmsgMcapIntrometryFixture, Flush)
{
    const intrometry_tests::ArilesDebug debug{};
    this->intrometry_sink_->assign(debug, intrometry::Source::Parameters(/*persistent_structure=*/true));

    this->intrometry_sink_->write(debug);
    this->intrometry_sink_->flush();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    this->intrometry_sink_->retract(debug);
    this->intrometry_sink_ = nullptr;
    ASSERT_NO_THROW(ASSERT_TRUE(intrometry_tests::checkMcap(this->directory_, this->sink_id_)));
}


int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
