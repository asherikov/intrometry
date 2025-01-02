# Copyright 2016 Proyectos y Sistemas de Mantenimiento SL (eProsima).
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

###############################################################################
# CMake build rules for FastCDR                                               #
###############################################################################
cmake_minimum_required(VERSION 3.20)

###############################################################################
# Project                                                                     #
###############################################################################
project(fastcdr VERSION 2.2.6 LANGUAGES CXX)

set(PROJECT_NAME_STYLED "FastCDR")
set(PROJECT_NAME_LARGE "Fast CDR")
string(TOUPPER "${PROJECT_NAME}" PROJECT_NAME_UPPER)
set(${PROJECT_NAME}_DESCRIPTION_SUMMARY "C++ library for serialize using CDR serialization")
set(${PROJECT_NAME}_DESCRIPTION "eProsima ${PROJECT_NAME_LARGE} library provides two serialization mechanisms. One is the standard CDR serialization mechanism, while the other is a faster implementation that modifies the standard.")

message(STATUS "Configuring ${PROJECT_NAME_LARGE}")
message(STATUS "Version: ${PROJECT_VERSION}")


###############################################################################
# Default shared libraries
###############################################################################
# Global flag to cause add_library() to create shared libraries if on.
# If set to true, this will cause all libraries to be built shared
# unless the library was explicitly added as a static library.
option(BUILD_SHARED_LIBS "Create shared libraries by default" OFF)

###############################################################################
# Test system configuration
###############################################################################
include(${PROJECT_SOURCE_DIR}/cmake/common/check_configuration.cmake)

set(FORCE_CXX "11" CACHE STRING "C++ standard fulfillment selection")
check_stdcxx(${FORCE_CXX})

check_endianness()
check_type_sizes()

###############################################################################
# Installation paths
###############################################################################
set(CMAKE_INSTALL_PREFIX "${CMAKE_CURRENT_BINARY_DIR}/fastcdr")

set(BIN_INSTALL_DIR bin/ CACHE PATH "Installation directory for binaries")
set(_include_dir "include/")
if(APPEND_PROJECT_NAME_TO_INCLUDEDIR)
  string(APPEND _include_dir "${PROJECT_NAME}/")
endif()
set(INCLUDE_INSTALL_DIR "${_include_dir}" CACHE PATH "Installation directory for C++ headers")
unset(_include_dir)
set(LIB_INSTALL_DIR lib${LIB_SUFFIX}/ CACHE PATH "Installation directory for libraries")
set(DATA_INSTALL_DIR share/ CACHE PATH "Installation directory for data")
if(WIN32)
    set(DOC_DIR "doc")
else()
    set(DOC_DIR "${DATA_INSTALL_DIR}/doc")
endif()
set(DOC_INSTALL_DIR ${DOC_DIR} CACHE PATH "Installation directory for documentation")
set(LICENSE_INSTALL_DIR ${DATA_INSTALL_DIR}/${PROJECT_NAME} CACHE PATH "Installation directory for licenses")

###############################################################################
# Compile library.
###############################################################################
add_subdirectory(src/cpp)

