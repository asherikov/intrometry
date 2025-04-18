cmake_minimum_required(VERSION 3.8)
project(intrometry_frontend VERSION 1.0.0 LANGUAGES CXX)

if(CCWS_CLANG_TIDY)
    set(CMAKE_CXX_CLANG_TIDY "${CCWS_CLANG_TIDY}" CACHE STRING "" FORCE)
endif()
if(CCWS_CXX_FLAGS)
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${CCWS_CXX_FLAGS}")
    set (CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${CCWS_LINKER_FLAGS}")
else()
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

find_package(ariles2-core REQUIRED)


add_library(${PROJECT_NAME} INTERFACE)
set_target_properties(${PROJECT_NAME} PROPERTIES EXPORT_NAME frontend)
target_link_libraries(${PROJECT_NAME}
    INTERFACE ariles2::core
)
target_include_directories(${PROJECT_NAME} INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_property(TARGET ${PROJECT_NAME} PROPERTY INTERFACE_${PROJECT_NAME}_MAJOR_VERSION ${PROJECT_VERSION_MAJOR})
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY COMPATIBLE_INTERFACE_STRING ${PROJECT_VERSION_MAJOR})


add_library(intrometry_backend STATIC
    src/backend.cpp
)
set_target_properties(intrometry_backend PROPERTIES EXPORT_NAME backend)

target_link_libraries(intrometry_backend
    PUBLIC ${PROJECT_NAME}
)


install(
    TARGETS ${PROJECT_NAME} intrometry_backend EXPORT ${PROJECT_NAME}
    INCLUDES DESTINATION include
)

install(DIRECTORY include/intrometry
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)


if(BUILD_TESTING)
    add_subdirectory(test)
endif()


# ---
# cmake package stuff
export(EXPORT ${PROJECT_NAME}
    FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake"
    NAMESPACE ${PROJECT_NAME}::
)

install(EXPORT ${PROJECT_NAME}
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE intrometry::
    DESTINATION share/${PROJECT_NAME}/
)


include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
    COMPATIBILITY SameMajorVersion
)
file(
    WRITE
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "include(\"\${CMAKE_CURRENT_LIST_DIR}/${PROJECT_NAME}Targets.cmake\")\n"
    "include(CMakeFindDependencyMacro)\n"
    "find_dependency(ariles2_core_ws)"
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION share/${PROJECT_NAME}/
)
# ---
