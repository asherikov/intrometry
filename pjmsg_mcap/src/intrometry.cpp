/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/


#include <ariles2/visitors/namevalue2.h>
#include <thread_supervisor/supervisor.h>

#include "HeaderCdrAux.ipp"
#include "StatisticsNamesCdrAux.ipp"
#include "StatisticsValuesCdrAux.ipp"
#include "TimeCdrAux.ipp"

#define MCAP_IMPLEMENTATION
#define MCAP_COMPRESSION_NO_LZ4
#define MCAP_COMPRESSION_NO_ZSTD
#include <mcap/writer.hpp>

#include "intrometry/intrometry.h"
#include "intrometry/backend/utils.h"
#include "intrometry/pjmsg_mcap/sink.h"

#include "messages.h"


namespace
{
    class NameValueContainer : public ariles2::namevalue2::NameValueContainer
    {
    public:
        plotjuggler_msgs::msg::StatisticsNames names_;
        plotjuggler_msgs::msg::StatisticsValues values_;
        std::size_t previous_size_ = 0;
        bool new_names_version_ = false;


    public:  // ariles stuff
        void finalize(const bool persistent_structure, const uint64_t timestamp, uint32_t &names_version)
        {
            const int32_t sec = static_cast<int32_t>(timestamp / std::nano::den);
            const uint32_t nanosec = timestamp % std::nano::den;

            names_.header().stamp().sec(sec);
            values_.header().stamp().sec(sec);
            names_.header().stamp().nanosec(nanosec);
            values_.header().stamp().nanosec(nanosec);

            // we cannot know for sure that the names have not changed
            // without comparing all the names, do our best
            if (not persistent_structure or previous_size_ != size())
            {
                names_.names_version(names_version);
                values_.names_version(names_version);
                new_names_version_ = true;
                ++names_version;
            }

            previous_size_ = size();
        }

        std::string &name(const std::size_t index)
        {
            return (names_.names()[index]);
        }
        double &value(const std::size_t index)
        {
            return (values_.values()[index]);
        }

        void reserve(const std::size_t size)
        {
            names_.names().reserve(size);
            values_.values().reserve(size);
        }
        [[nodiscard]] std::size_t size() const
        {
            return (names_.names().size());
        }
        void resize(const std::size_t size)
        {
            names_.names().resize(size);
            values_.values().resize(size);
        }
    };
}  // namespace


namespace
{
    class McapCDRWriter
    {
    protected:
        template <class t_Message>
        class Channel
        {
        protected:
            mcap::Message message_;
            eprosima::fastcdr::CdrSizeCalculator cdr_size_calculator_;

        protected:
            uint32_t getSize(const t_Message &message)
            {
                size_t current_alignment{ 0 };
                return (cdr_size_calculator_.calculate_serialized_size(message, current_alignment)
                        + 4u /*encapsulation*/);
            }

        public:
            Channel() : cdr_size_calculator_(eprosima::fastcdr::CdrVersion::XCDRv1)
            {
            }

            void initialize(mcap::McapWriter &writer, const std::string_view &msg_topic)
            {
                mcap::Schema schema(
                        intrometry_private::pjmsg_mcap::Message<t_Message>::type,
                        "ros2msg",
                        intrometry_private::pjmsg_mcap::Message<t_Message>::schema);
                writer.addSchema(schema);

                mcap::Channel channel(msg_topic, "ros2msg", schema.id);
                writer.addChannel(channel);

                message_.channelId = channel.id;
            }

            void write(mcap::McapWriter &writer, std::vector<std::byte> &buffer, const t_Message &message)
            {
                buffer.resize(getSize(message));
                message_.data = buffer.data();

                {
                    eprosima::fastcdr::FastBuffer cdr_buffer(
                            reinterpret_cast<char *>(buffer.data()), buffer.size());  // NOLINT
                    eprosima::fastcdr::Cdr ser(
                            cdr_buffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::CdrVersion::XCDRv1);
                    ser.set_encoding_flag(eprosima::fastcdr::EncodingAlgorithmFlag::PLAIN_CDR);

                    ser.serialize_encapsulation();
                    ser << message;
                    ser.set_dds_cdr_options({ 0, 0 });

                    message_.dataSize = ser.get_serialized_data_length();
                }

                message_.logTime = intrometry::backend::now();
                message_.publishTime = message_.logTime;


                const mcap::Status res = writer.write(message_);
                if (not res.ok())
                {
                    throw std::runtime_error(
                            intrometry::backend::str_concat("Failed to write a message: ", res.message));
                }
            }
        };

    public:
        std::tuple<Channel<plotjuggler_msgs::msg::StatisticsNames>, Channel<plotjuggler_msgs::msg::StatisticsValues>>
                channels_;

        std::vector<std::byte> buffer_;
        mcap::McapWriter writer_;

    public:
        ~McapCDRWriter()
        {
            writer_.close();
        }

        void initialize(const std::filesystem::path &filename, const std::string &topic_prefix)
        {
            {
                const mcap::McapWriterOptions options = mcap::McapWriterOptions("ros2msg");
                const mcap::Status res = writer_.open(filename.native(), options);
                if (not res.ok())
                {
                    throw std::runtime_error(intrometry::backend::str_concat(
                            "Failed to open ", filename.native(), " for writing: ", res.message));
                }
            }

            std::get<Channel<plotjuggler_msgs::msg::StatisticsNames>>(channels_).initialize(
                    writer_, intrometry::backend::str_concat(topic_prefix, "/names"));

            std::get<Channel<plotjuggler_msgs::msg::StatisticsValues>>(channels_).initialize(
                    writer_, intrometry::backend::str_concat(topic_prefix, "/values"));
        }

        template <class t_Message>
        void write(const t_Message &message)
        {
            std::get<Channel<t_Message>>(channels_).write(writer_, buffer_, message);
        }
    };
}  // namespace

namespace
{
    class WriterWrapper
    {
    public:
        std::mutex mutex_;
        ariles2::namevalue2::Writer::Parameters writer_parameters_;
        std::shared_ptr<NameValueContainer> data_;
        ariles2::namevalue2::Writer writer_;

        bool serialized_;

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
            data_->finalize(writer_parameters_.persistent_structure_, 0, names_version);

            serialized_ = true;  // do not serialize on assignment
        }

        void serialize(McapCDRWriter &mcap_writer)
        {
            if (mutex_.try_lock())
            {
                if (not serialized_)
                {
                    if (data_->new_names_version_)
                    {
                        mcap_writer.write(data_->names_);
                        data_->new_names_version_ = false;
                    }

                    mcap_writer.write(data_->values_);
                    serialized_ = true;
                }
                mutex_.unlock();
            }
        }

        void write(const ariles2::DefaultBase &source, const uint64_t timestamp, uint32_t &names_version)
        {
            if (mutex_.try_lock())
            {
                ariles2::apply(writer_, source);
                data_->finalize(writer_parameters_.persistent_structure_, timestamp, names_version);
                serialized_ = false;

                mutex_.unlock();
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
        tut::thread::Supervisor<> thread_supervisor_;
        uint32_t names_version_;

        intrometry::backend::SourceContainer<WriterWrapper> sources_;

        McapCDRWriter mcap_writer_;

    public:
        Implementation(const std::filesystem::path &directory, const std::string &sink_id, const std::size_t rate)
        {
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


        void assign(const ariles2::DefaultBase &source, const Source::Parameters &parameters)
        {
            sources_.tryEmplace(source, parameters.persistent_structure_, names_version_);
        }

        void retract(const ariles2::DefaultBase &source)
        {
            sources_.erase(source);
        }

        void write(const ariles2::DefaultBase &source, const uint64_t timestamp)
        {
            if (not sources_.tryVisit(
                        source,
                        [this, &source, timestamp](WriterWrapper &writer) {
                            writer.write(
                                    source, (0 == timestamp) ? intrometry::backend::now() : timestamp, names_version_);
                        }))
            {
                thread_supervisor_.log(
                        "Measurement source handler is not assigned, skipping id: ", source.arilesDefaultID());
            }
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


    void Sink::assign(const ariles2::DefaultBase &source, const Source::Parameters &parameters)
    {
        if (pimpl_)
        {
            pimpl_->assign(source, parameters);
        }
    }


    void Sink::retract(const ariles2::DefaultBase &source)
    {
        if (pimpl_)
        {
            pimpl_->retract(source);
        }
    }


    void Sink::write(const ariles2::DefaultBase &source, const uint64_t timestamp)
    {
        if (pimpl_)
        {
            pimpl_->write(source, timestamp);
        }
    }
}  // namespace intrometry::pjmsg_mcap
