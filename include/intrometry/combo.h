/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)

    @brief Combo publisher: publisher + data container
*/

#pragma once

#include <tuple>

#include "publisher.h"


namespace intrometry
{
    /**
     * @brief Combo publisher: publisher + data container
     *
     * A helper class that contains both a publisher and multiple source
     * instances that are automatically assigned to it on initialization.
     *
     * @ingroup API
     */
    template <class... t_Ariles>
    class ComboPublisher
    {
    public:
        Publisher publisher_;
        std::tuple<t_Ariles...> data_;

    public:
        /// Initialize publisher and assign sources.
        template <class... t_Args>
        bool initialize(const Source::Parameters &parameters, t_Args &&...args)
        {
            if (not publisher_.initialize(std::forward<t_Args>(args)...))
            {
                return (false);
            }
            std::apply(
                    [this, parameters](auto &&...assign_args) { publisher_.assignBatch(parameters, assign_args...); },
                    data_);
            return (true);
        }

        /// Get data source by its type
        template <class t_Data>
        t_Data &get()
        {
            return (std::get<t_Data>(data_));
        }

        /// Write all sources to publisher
        void write(const uint64_t timestamp = 0)
        {
            std::apply(
                    [this, timestamp](auto &&...write_args) { publisher_.writeBatch(timestamp, write_args...); },
                    data_);
        }
    };
}  // namespace intrometry
