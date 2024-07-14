/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#pragma once

#include <tuple>

#include "publisher.h"


namespace intrometry
{
    template <class... t_Ariles>
    class ComboPublisher
    {
    public:
        Publisher publisher_;
        std::tuple<t_Ariles...> data_;

    public:
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

        template <class t_Data>
        t_Data &get()
        {
            return (std::get<t_Data>(data_));
        }

        void write(const uint64_t timestamp = 0)
        {
            std::apply(
                    [this, timestamp](auto &&...write_args) { publisher_.writeBatch(timestamp, write_args...); },
                    data_);
        }
    };
}  // namespace intrometry
