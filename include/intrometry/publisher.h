/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)

    @brief Publisher class.
*/

#pragma once

#include <string>
#include <memory>

#include "source.h"


namespace intrometry
{
    /**
     * @brief Publish data.
     *
     * @ingroup API
     */
    class Publisher
    {
    public:
        class Parameters
        {
        public:
            /**
             * publish rate (system clock),
             * data written at higher rate is going to overwrite unpublished data
             */
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

        /**
         * Initialize publisher.
         * @return true on success
         */
        bool initialize(
                /// id of the publisher, disables publishing if empty
                const std::string &publisher_id,
                const Parameters &parameters = Parameters());

        /**
         * Assign data source (an ariles class) with parameters. Data source is
         * identified by default ariles id (ARILES2_DEFAULT_ID), which is set
         * to "ariles" by default -- you have to override it in ariles class
         * definition to avoid naming collisions.
         *
         * @note Does nothing if initialization failed.
         */
        void assign(const ariles2::DefaultBase &source, const Source::Parameters &parameters = Source::Parameters());

        /**
         * Unassign source.
         *
         * @note Source may be retracted before any data is published.
         * @note Does nothing if initialization failed.
         * @note Does nothing if the source has not been assigned.
         */
        void retract(const ariles2::DefaultBase &source);

        /**
         * Write data. Data is copied to internal buffers, and scheduled for
         * publication.
         *
         * @note Does nothing if source is unassigned.
         * @note Does nothing if initialization failed.
         */
        void write(const ariles2::DefaultBase &source, const uint64_t timestamp = 0);


        /// Batch assignment
        template <class... t_Sources>
        void assignBatch(const Source::Parameters &parameters, t_Sources &&...sources)
        {
            ((void)assign(std::forward<t_Sources>(sources), parameters), ...);
        }

        /// Batch retraction
        template <class... t_Sources>
        void retractBatch(t_Sources &&...sources)
        {
            ((void)retract(std::forward<t_Sources>(sources)), ...);
        }

        /// Batch writing
        template <class... t_Sources>
        void writeBatch(const uint64_t timestamp, t_Sources &&...sources)
        {
            ((void)write(std::forward<t_Sources>(sources), timestamp), ...);
        }
    };
}  // namespace intrometry
