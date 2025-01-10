/**
    @file
    @author  Alexander Sherikov
    @copyright 2025 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)

    @brief Source parameters and necessary ariles inclusions.
*/

#pragma once

#include <memory>

namespace intrometry::backend
{
    template <typename... t_String>
    std::string str_concat(t_String &&...strings)
    {
        std::string result;
        (result += ... += strings);
        return result;
    }

    uint64_t now();
    uint32_t getRandomUInt32();

    std::string getRandomId(const std::size_t length);
    std::string normalizeId(const std::string &input_id);


    class RateTimer
    {
    protected:
        class Implementation;

    protected:
        const std::unique_ptr<Implementation> pimpl_;

    public:
        explicit RateTimer(const std::size_t rate);
        ~RateTimer();
        [[nodiscard]] bool valid() const;
        void start();
        void step();
    };
}  // namespace intrometry::backend
