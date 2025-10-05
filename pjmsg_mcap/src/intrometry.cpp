/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/


#include <ariles2/visitors/namevalue2.h>
#include <thread_supervisor/supervisor.h>

#include <pjmsg_mcap_wrapper/all.h>

#include "intrometry/intrometry.h"
#include "intrometry/backend/utils.h"
#include "intrometry/pjmsg_mcap/sink.h"


namespace
{
    class NameValueContainer : public ariles2::namevalue2::NameValueContainer
    {
    public:
        std::size_t previous_size_ = 0;
        bool new_names_version_ = false;
        pjmsg_mcap_wrapper::Message message_;


    public:  // ariles stuff
        void finalize(const bool persistent_structure, const uint64_t timestamp, uint32_t &names_version)
        {
            message_.setStamp(timestamp);

            // we cannot know for sure that the names have not changed
            // without comparing all the names, do our best
            if (not persistent_structure or previous_size_ != message_.size())
            {
                message_.setVersion(names_version);
                ++names_version;
            }

            previous_size_ = message_.size();
        }

        std::string &name(const std::size_t index)
        {
            return (message_.name(index));
        }

        double &value(const std::size_t index)
        {
            return (message_.value(index));
        }

        void reserve(const std::size_t size)
        {
            message_.reserve(size);
        }

        void resize(const std::size_t size)
        {
            message_.resize(size);
        }

        [[nodiscard]] std::size_t size() const
        {
            return (message_.size());
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

        bool serialized_;

    public:
        WriterWrapper(
                const ariles2::DefaultBase &source,
                std::string id,
                const bool persistent_structure,
                uint32_t &names_version)
          : id_(std::move(id)), data_(std::make_shared<NameValueContainer>()), writer_(data_)
        {
            writer_parameters_ = writer_.getDefaultParameters();
            if (persistent_structure)
            {
                writer_parameters_.persistent_structure_ = true;
            }

            // write to allocate memory
            ariles2::apply(writer_, source, id_);
            data_->finalize(writer_parameters_.persistent_structure_, 0, names_version);

            serialized_ = true;  // do not serialize on assignment
        }

        void serialize(pjmsg_mcap_wrapper::Writer &mcap_writer)
        {
            if (not serialized_)
            {
                mcap_writer.write(data_->message_);
                serialized_ = true;
            }
        }

        void write(const ariles2::DefaultBase &source, const uint64_t timestamp, uint32_t &names_version)
        {
            ariles2::apply(writer_, source, id_);
            data_->finalize(writer_parameters_.persistent_structure_, timestamp, names_version);
            serialized_ = false;
        }
    };
}  // namespace


namespace intrometry::pjmsg_mcap::sink
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

    Parameters &Parameters::directory(const std::filesystem::path &value)
    {
        directory_ = value;
        return (*this);
    }
}  // namespace intrometry::pjmsg_mcap::sink


namespace intrometry::pjmsg_mcap::sink
{
    class Implementation
    {
    protected:
        pjmsg_mcap_wrapper::Writer mcap_writer_;

    public:
        uint32_t names_version_;

        intrometry::backend::SourceContainer<WriterWrapper> sources_;
        tut::thread::Supervisor<> thread_supervisor_;

    public:
        Implementation(const std::filesystem::path &directory, const std::string &sink_id, const std::size_t rate)
        {
            names_version_ = intrometry::backend::getRandomUInt32();

            const std::string node_id = intrometry::backend::normalizeId(sink_id);
            const std::string random_id = intrometry::backend::getRandomId(8);
            const std::string topic_prefix =
                    intrometry::backend::str_concat("/intrometry/", node_id.empty() ? random_id : node_id);

            if (not directory.empty())
            {
                std::filesystem::create_directories(directory);
            }
            const std::filesystem::path filename = directory
                                                   / intrometry::backend::str_concat(
                                                           node_id,
                                                           node_id.empty() ? "" : "_",
                                                           random_id,
                                                           "_",
                                                           intrometry::backend::getDateString(),
                                                           ".mcap");

            mcap_writer_.initialize(filename, topic_prefix);

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

                while (not thread_supervisor_.isInterrupted())
                {
                    sources_.tryVisit([this](WriterWrapper &writer) { writer.serialize(mcap_writer_); });

                    timer.step();
                }
            }
            else
            {
                thread_supervisor_.log("Incorrect spin rate");
            }
            thread_supervisor_.interrupt();
        }
    };
}  // namespace intrometry::pjmsg_mcap::sink


namespace intrometry::pjmsg_mcap
{
    Sink::~Sink() = default;


    bool Sink::initialize()
    {
        if (parameters_.id_.empty())
        {
            return (false);
        }
        make_pimpl(parameters_.directory_, parameters_.id_, parameters_.rate_);
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
            if (not pimpl_->sources_.tryVisit(
                        id,
                        source,
                        [this, &source, &timestamp](WriterWrapper &writer) {
                            writer.write(
                                    source,
                                    (0 == timestamp) ? intrometry::backend::now() : timestamp,
                                    pimpl_->names_version_);
                        }))
            {
                pimpl_->thread_supervisor_.log(
                        "Measurement source handler is not assigned, skipping id: ", source.arilesDefaultID());
            }
        }
    }
}  // namespace intrometry::pjmsg_mcap
