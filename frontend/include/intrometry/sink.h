/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)

    @brief Sink class.
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
    class SinkBase
    {
    public:
        virtual ~SinkBase() = default;

        /**
         * Initialize sink.
         * @return true on success
         */
        virtual bool initialize() = 0;

        /**
         * Assign data source (an ariles class) with parameters. Data source is
         * identified by default ariles id (ARILES2_DEFAULT_ID), which is set
         * to "ariles" by default -- you have to override it in ariles class
         * definition to avoid naming collisions.
         *
         * @note Does nothing if initialization failed.
         */
        virtual void assign(
                const ariles2::DefaultBase &source,
                const Source::Parameters &parameters = Source::Parameters()) = 0;

        /**
         * Unassign source.
         *
         * @note Source may be retracted before any data is published.
         * @note Does nothing if initialization failed.
         * @note Does nothing if the source has not been assigned.
         */
        virtual void retract(const ariles2::DefaultBase &source) = 0;

        /**
         * Write data. Data is copied to internal buffers, and scheduled for
         * publication.
         *
         * @note Does nothing if source is unassigned.
         * @note Does nothing if initialization failed.
         */
        virtual void write(const ariles2::DefaultBase &source, const uint64_t timestamp = 0) = 0;


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


    template <class t_Parameters, class t_Implementation>
    class SinkPIMPLBase : public SinkBase
    {
    public:
        using Parameters = t_Parameters;

    protected:
        t_Parameters parameters_;
        std::shared_ptr<t_Implementation> pimpl_;

    public:
        SinkPIMPLBase(const t_Parameters &parameters = t_Parameters()) : parameters_(parameters)  // NOLINT
        {
        }

    protected:
        template <class... t_Args>
        void make_pimpl(t_Args &&...args)
        {
            pimpl_ = std::make_shared<t_Implementation>(std::forward<t_Args>(args)...);
        }
    };
}  // namespace intrometry
