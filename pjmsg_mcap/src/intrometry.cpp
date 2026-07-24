/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/


#include <ariles2/visitors/namevalue2.h>
#include <atomic>
#include <thread_supervisor/supervisor.h>

#include <pjmsg_mcap_wrapper/writer.h>

#include "intrometry/intrometry.h"
#include "intrometry/backend/utils.h"
#include "intrometry/pjmsg_mcap/sink.h"


namespace
{
    std::string getDateString()
    {
        const std::time_t date_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::stringstream date_stream;
        // thread-unsafe
        date_stream << std::put_time(std::gmtime(&date_now), "%Y%m%d_%H%M%S");  // NOLINT

        return (date_stream.str());
    }


    class NameValueContainer : public ariles2::namevalue2::NameValueContainer
    {
    public:
        std::size_t previous_size_ = 0;
        std::shared_ptr<pjmsg_mcap_wrapper::Message> message_in_;
        std::shared_ptr<pjmsg_mcap_wrapper::Message> message_out_;


    public:
        NameValueContainer()
        {
            message_in_ = std::make_shared<pjmsg_mcap_wrapper::Message>();
            message_out_ = std::make_shared<pjmsg_mcap_wrapper::Message>();
        }

        void swap()
        {
            if (message_out_->getVersion() != message_in_->getVersion())
            {
                message_out_->reset(*message_in_);
            }
            std::swap(message_out_, message_in_);
        }

        void finalize(const bool persistent_structure, const uint64_t timestamp, std::atomic<uint32_t> &names_version)
        {
            message_in_->setStamp(timestamp);

            // we cannot know for sure that the names have not changed
            // without comparing all the names, do our best
            if (not persistent_structure or previous_size_ != message_in_->size())
            {
                // fetch_add atomically returns the old value and increments,
                // preventing concurrent writes from getting the same version.
                message_in_->setVersion(names_version.fetch_add(1));
            }

            previous_size_ = message_in_->size();
        }

        std::string &name(const std::size_t index)
        {
            return (message_in_->name(index));
        }

        double &value(const std::size_t index)
        {
            return (message_in_->value(index));
        }

        void reserve(const std::size_t size)
        {
            message_in_->reserve(size);
        }

        void resize(const std::size_t size)
        {
            message_in_->resize(size);
        }

        [[nodiscard]] std::size_t size() const
        {
            return (message_in_->size());
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

        std::mutex mutex_in_;
        std::mutex mutex_out_;
        std::atomic<bool> flushed_;

    public:
        WriterWrapper(
                const ariles2::DefaultBase &source,
                std::string id,
                const bool persistent_structure,
                std::atomic<uint32_t> &names_version)
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

            flushed_ = true;  // do not serialize on assignment
        }

        void serialize(pjmsg_mcap_wrapper::Writer &mcap_writer)
        {
            if (not flushed_)
            {
                if (mutex_in_.try_lock() && mutex_out_.try_lock())
                {
                    data_->swap();
                    mutex_in_.unlock();
                    mcap_writer.write(*(data_->message_out_));
                    flushed_ = true;
                    mutex_out_.unlock();
                }
            }
        }

        void write(const ariles2::DefaultBase &source, const uint64_t timestamp, std::atomic<uint32_t> &names_version)
        {
            if (mutex_in_.try_lock())
            {
                ariles2::apply(writer_, source, id_);
                data_->finalize(writer_parameters_.persistent_structure_, timestamp, names_version);
                flushed_ = false;
                mutex_in_.unlock();
            }
        }
    };
}  // namespace


namespace intrometry::pjmsg_mcap::sink
{
    Parameters::Parameters(const std::string &id)
    {
        rate_ = 500;
        id_ = id;
        compression_ = Compression::NONE;
    }

    Parameters::Parameters(const char *id)
    {
        rate_ = 500;
        id_ = id;
        compression_ = Compression::NONE;
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

    Parameters &Parameters::compression(const Compression value)
    {
        compression_ = value;
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
        std::atomic<uint32_t> names_version_;

        intrometry::backend::SourceContainer<WriterWrapper> sources_;
        tut::thread::Supervisor<> thread_supervisor_;

    public:
        Implementation(
                const std::filesystem::path &directory,
                const std::string &sink_id,
                const std::size_t rate,
                const Parameters::Compression compression)
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
            const std::filesystem::path filename =
                    directory
                    / intrometry::backend::str_concat(
                            node_id, node_id.empty() ? "" : "_", getDateString(), "_", random_id, ".mcap");

            // Create writer parameters with the specified compression
            pjmsg_mcap_wrapper::Writer::Parameters writer_params;
            if (Parameters::Compression::ZSTD == compression)
            {
                writer_params.compression_ = pjmsg_mcap_wrapper::Writer::Parameters::Compression::ZSTD;
            }
            mcap_writer_.initialize(filename, topic_prefix, writer_params);

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
                    flush();

                    timer.step();
                }
                flush();
            }
            else
            {
                thread_supervisor_.log("Incorrect spin rate");
            }
            thread_supervisor_.interrupt();
        }


        void flush()
        {
            sources_.tryFlush([this](WriterWrapper &writer) { writer.serialize(mcap_writer_); });
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
        make_pimpl(parameters_.directory_, parameters_.id_, parameters_.rate_, parameters_.compression_);
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
            if (not pimpl_->sources_.tryWrite(
                        id,
                        source,
                        [this, &source, &timestamp](WriterWrapper &writer)
                        {
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


    void Sink::flush()
    {
        if (pimpl_)
        {
            pimpl_->flush();
        }
    }
}  // namespace intrometry::pjmsg_mcap
