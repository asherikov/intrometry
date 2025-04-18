/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)

    @brief Sink class.
*/

#pragma once

#include <intrometry/sink.h>
#include <intrometry/backend/utils.h>


namespace intrometry::pjmsg_topic
{
    namespace sink
    {
        class INTROMETRY_PUBLIC Parameters
        {
        public:
            /**
             * publish rate (system clock),
             * data written at higher rate is going to overwrite unpublished data
             */
            std::size_t rate_;
            /// id of the sink, disables publishing if empty
            std::string id_;

        public:
            Parameters(const std::string &id = "");  // NOLINT
            Parameters(const char *id = "");         // NOLINT

            Parameters &rate(const std::size_t value);
            Parameters &id(const std::string &value);
        };

        class Implementation;
    }  // namespace sink


    /**
     * @brief Publish data.
     */
    class INTROMETRY_PUBLIC Sink : public SinkPIMPLBase<sink::Parameters, sink::Implementation>
    {
    public:
        using SinkPIMPLBase::SinkPIMPLBase;
        using SinkPIMPLBase::assign;
        using SinkPIMPLBase::write;
        using SinkPIMPLBase::retract;
        ~Sink();

        bool initialize();
        void assign(
                const std::string &id,
                const ariles2::DefaultBase &source,
                const Source::Parameters &parameters = Source::Parameters());
        void retract(const std::string &id, const ariles2::DefaultBase &source);
        void write(const std::string &id, const ariles2::DefaultBase &source, const uint64_t timestamp = 0);
    };
}  // namespace intrometry::pjmsg_topic
