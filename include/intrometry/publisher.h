/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#pragma once

#include <string>
#include <memory>

#include "source.h"


namespace intrometry
{
    class Publisher
    {
    public:
        class Parameters
        {
        public:
            /// publish rate (system clock), data written at higher rate is going to be lost
            std::size_t rate_;

        public:
            Parameters()  // NOLINT
            {
                rate_ = 500;
            }

            Parameters &rate(const std::size_t value)
            {
                rate_ = value;
                return (*this);
            }
        };


    protected:
        class Implementation;


    protected:
        std::shared_ptr<Implementation> pimpl_;


    public:
        Publisher();
        ~Publisher();

        bool initialize(
                /// id of the publisher, disabled if empty
                const std::string &publisher_id,
                const Parameters &parameters = Parameters());

        void assign(const ariles2::DefaultBase &source, const Source::Parameters &parameters = Source::Parameters());
        /// note that source may get retracted before the data is published
        void retract(const ariles2::DefaultBase &source);
        void write(const ariles2::DefaultBase &source, const uint64_t timestamp = 0);


        template <class... t_Sources>
        void assignBatch(const Source::Parameters &parameters, t_Sources &&...sources)
        {
            ((void)assign(std::forward<t_Sources>(sources), parameters), ...);
        }

        template <class... t_Sources>
        void retractBatch(t_Sources &&...sources)
        {
            ((void)retract(std::forward<t_Sources>(sources)), ...);
        }

        template <class... t_Sources>
        void writeBatch(const uint64_t timestamp, t_Sources &&...sources)
        {
            ((void)write(std::forward<t_Sources>(sources), timestamp), ...);
        }
    };
}  // namespace intrometry
