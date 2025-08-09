cmake_minimum_required(VERSION 3.8)
project(intrometry_tests VERSION 1.0.0 LANGUAGES CXX)

include(ccws_set_cmake_flags OPTIONAL RESULT_VARIABLE CCWS_ON)
if(NOT CCWS_ON)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
    set(CMAKE_VERBOSE_MAKEFILE ON)

    if(NOT CMAKE_CXX_STANDARD)
        set(CMAKE_CXX_STANDARD 17)
        set(CMAKE_CXX_STANDARD_REQUIRED ON)
    endif()

    if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        add_compile_options(-Wall -Wextra -Wpedantic -Werror)
    endif()
endif()

if(NOT DEFINED BUILD_TESTING OR BUILD_TESTING)
    enable_testing()
    add_subdirectory(test)
endif()
