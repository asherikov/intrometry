/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#include <unordered_map>

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

#include "schema_names.h"
#include "schema_values.h"


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
    public:
        mcap::Message mcap_message_names_;
        mcap::Message mcap_message_values_;

        std::vector<std::byte> buffer_;
        eprosima::fastcdr::CdrSizeCalculator cdr_size_calculator_;
        mcap::McapWriter writer_;

    public:
        McapCDRWriter() : cdr_size_calculator_(eprosima::fastcdr::CdrVersion::XCDRv1)
        {
        }

        ~McapCDRWriter()
        {
            writer_.close();
        }

        void initialize(const std::string &filename, const std::string &topic_prefix)  // NOLINT
        {
            {
                const mcap::McapWriterOptions options = mcap::McapWriterOptions("ros2msg");
                const mcap::Status res = writer_.open(filename, options);
                if (not res.ok())
                {
                    throw std::runtime_error(intrometry::backend::str_concat(
                            "Failed to open ", filename, " for writing: ", res.message));
                }
            }

            {
                mcap::Schema schema(
                        "plotjuggler_msgs/msg/StatisticsNames",
                        "ros2msg",
                        intrometry_private::pjmsg_mcap::schema::names);
                writer_.addSchema(schema);

                mcap::Channel channel(intrometry::backend::str_concat(topic_prefix, "/names"), "ros2msg", schema.id);
                writer_.addChannel(channel);

                mcap_message_names_.channelId = channel.id;
            }

            {
                mcap::Schema schema(
                        "plotjuggler_msgs/msg/StatisticsValues",
                        "ros2msg",
                        intrometry_private::pjmsg_mcap::schema::values);
                writer_.addSchema(schema);

                mcap::Channel channel(intrometry::backend::str_concat(topic_prefix, "/values"), "ros2msg", schema.id);
                writer_.addChannel(channel);

                mcap_message_values_.channelId = channel.id;
            }
        }

        template <class t_Message>
        uint32_t getSize(const t_Message &message)
        {
            size_t current_alignment{ 0 };
            return (cdr_size_calculator_.calculate_serialized_size(message, current_alignment) + 4u /*encapsulation*/);
        }

        template <class t_Message>
        void write(mcap::Message &mcap_message, const t_Message &message)
        {
            buffer_.resize(getSize(message));
            mcap_message.data = buffer_.data();

            {
                eprosima::fastcdr::FastBuffer cdr_buffer(
                        reinterpret_cast<char *>(buffer_.data()), buffer_.size());  // NOLINT
                eprosima::fastcdr::Cdr ser(
                        cdr_buffer, eprosima::fastcdr::Cdr::DEFAULT_ENDIAN, eprosima::fastcdr::CdrVersion::XCDRv1);
                ser.set_encoding_flag(eprosima::fastcdr::EncodingAlgorithmFlag::PLAIN_CDR);

                ser.serialize_encapsulation();
                ser << message;
                ser.set_dds_cdr_options({ 0, 0 });

                mcap_message.dataSize = ser.get_serialized_data_length();
            }

            mcap_message.logTime = intrometry::backend::now();
            mcap_message.publishTime = mcap_message.logTime;


            const mcap::Status res = writer_.write(mcap_message);
            if (not res.ok())
            {
                throw std::runtime_error(intrometry::backend::str_concat("Failed to write a message: ", res.message));
            }
        }

        template <class t_Message>
        void writeValues(const t_Message &message)
        {
            write(mcap_message_values_, message);
        }

        template <class t_Message>
        void writeNames(const t_Message &message)
        {
            write(mcap_message_names_, message);
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
                        mcap_writer.writeNames(data_->names_);
                        data_->new_names_version_ = false;
                    }

                    mcap_writer.writeValues(data_->values_);
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
}  // namespace intrometry::pjmsg_mcap::sink

namespace intrometry::pjmsg_mcap::sink
{
    class Implementation
    {
    protected:
        using SourceSet = std::unordered_map<std::string, WriterWrapper>;


    protected:
        std::mutex update_mutex_;
        std::mutex write_mutex_;

        tut::thread::Supervisor<> thread_supervisor_;
        uint32_t names_version_;

        SourceSet sources_;

        McapCDRWriter mcap_writer_;

    public:
        Implementation(const std::string &sink_id, const std::size_t rate)
        {
            const std::string node_id = intrometry::backend::normalizeId(sink_id);
            const std::string random_id = intrometry::backend::getRandomId(8);
            const std::string id =
                    node_id.empty() ? random_id : intrometry::backend::str_concat(node_id, "_", random_id);
            const std::string topic_prefix =
                    intrometry::backend::str_concat("/intrometry/", node_id.empty() ? random_id : node_id);
            const std::string filename = intrometry::backend::str_concat(id, ".mcap");


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
                    if (write_mutex_.try_lock())
                    {
                        for (std::pair<const std::string, WriterWrapper> &source : sources_)
                        {
                            source.second.serialize(mcap_writer_);
                        }

                        write_mutex_.unlock();
                    }

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
            const std::lock_guard<std::mutex> update_lock(update_mutex_);
            const std::lock_guard<std::mutex> write_lock(write_mutex_);

            sources_.try_emplace(source.arilesDefaultID(), source, parameters.persistent_structure_, names_version_);
        }

        void retract(const ariles2::DefaultBase &source)
        {
            const std::lock_guard<std::mutex> update_lock(update_mutex_);
            const std::lock_guard<std::mutex> write_lock(write_mutex_);

            sources_.erase(source.arilesDefaultID());
        }

        void write(const ariles2::DefaultBase &source, const uint64_t timestamp)
        {
            if (update_mutex_.try_lock())
            {
                const SourceSet::iterator source_it = sources_.find(source.arilesDefaultID());

                if (sources_.end() == source_it)
                {
                    thread_supervisor_.log(
                            "Measurement source handler is not assigned, skipping id: ", source.arilesDefaultID());
                }
                else
                {
                    if (0 == timestamp)
                    {
                        source_it->second.write(source, intrometry::backend::now(), names_version_);
                    }
                    else
                    {
                        source_it->second.write(source, timestamp, names_version_);
                    }
                }

                update_mutex_.unlock();
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
        make_pimpl(parameters_.id_, parameters_.rate_);
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
