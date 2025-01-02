/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)

    @brief Combo sink: sink + data container
*/

#pragma once

#include <tuple>

#include "sink.h"


namespace intrometry
{
    /**
     * @brief Combo sink: sink + data container
     *
     * A helper class that contains both a sink and multiple source
     * instances that are automatically assigned to it on initialization.
     *
     * @ingroup API
     */
    template <class... t_Ariles>
    class ComboSink
    {
    public:
        std::shared_ptr<SinkBase> sink_;
        std::tuple<t_Ariles...> data_;

    public:
        /// Initialize sink and assign sources.
        template <class t_Sink, class... t_Args>
        bool initialize(
                const Source::Parameters &source_parameters,
                t_Args &&...args)
        {
            sink_ = std::make_shared<t_Sink>(std::forward<t_Args>(args)...);
            if (not sink_->initialize())
            {
                return (false);
            }
            std::apply(
                    [this, source_parameters](auto &&...assign_args)
                    { sink_->assignBatch(source_parameters, assign_args...); },
                    data_);
            return (true);
        }

        /// Get data source by its type
        template <class t_Data>
        t_Data &get()
        {
            return (std::get<t_Data>(data_));
        }

        /// Write all sources to sink
        void write(const uint64_t timestamp = 0)
        {
            std::apply(
                    [this, timestamp](auto &&...write_args) { sink_->writeBatch(timestamp, write_args...); },
                    data_);
        }
    };
}  // namespace intrometry
