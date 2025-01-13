/**
    @file
    @author  Alexander Sherikov
    @copyright 2024 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#pragma once


#include <chrono>
#include <thread>

#include <gtest/gtest.h>

#include <thread_supervisor/supervisor.h>
#include <ariles2/adapters/std_vector.h>


namespace intrometry_tests
{
    class ArilesDebug : public ariles2::DefaultBase
    {
#define ARILES2_DEFAULT_ID "ArilesDebug"
#define ARILES2_ENTRIES(v)                                                                                             \
    ARILES2_TYPED_ENTRY_(v, duration, double)                                                                          \
    ARILES2_TYPED_ENTRY_(v, size, std::size_t)                                                                         \
    ARILES2_TYPED_ENTRY_(v, vec, std::vector<float>)
#include ARILES2_INITIALIZE
    public:
        virtual ~ArilesDebug() = default;
    };

    class ArilesDebug1 : public ArilesDebug
    {
#define ARILES2_DEFAULT_ID "ArilesDebug1"
#define ARILES2_ENTRIES(v) ARILES2_PARENT(v, ArilesDebug)
#include ARILES2_INITIALIZE
    public:
        virtual ~ArilesDebug1() = default;
    };
}  // namespace intrometry_tests
