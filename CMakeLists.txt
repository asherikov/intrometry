cmake_minimum_required(VERSION 3.8)
project(intrometry VERSION 0.2.0 LANGUAGES CXX)

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

find_package(rclcpp REQUIRED)
find_package(pal_statistics_msgs REQUIRED)
find_package(thread_supervisor REQUIRED)
find_package(ariles2_namevalue2_ws REQUIRED)


add_library(${PROJECT_NAME} SHARED
    src/${PROJECT_NAME}.cpp
)
target_link_libraries(${PROJECT_NAME}
    PUBLIC ariles2_visitor_namevalue2
    PRIVATE thread_supervisor::thread_supervisor
    PRIVATE rclcpp::rclcpp
    PRIVATE pal_statistics_msgs::pal_statistics_msgs__rosidl_typesupport_cpp
)
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)
set_property(TARGET ${PROJECT_NAME} PROPERTY INTERFACE_${PROJECT_NAME}_MAJOR_VERSION ${PROJECT_VERSION_MAJOR})
set_property(TARGET ${PROJECT_NAME} APPEND PROPERTY COMPATIBLE_INTERFACE_STRING ${PROJECT_VERSION_MAJOR})

install(
    TARGETS ${PROJECT_NAME} EXPORT ${PROJECT_NAME}
    INCLUDES DESTINATION include
)

install(TARGETS ${PROJECT_NAME}
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/${PROJECT_NAME}
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
    NAMESPACE ${PROJECT_NAME}::
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
    "find_dependency(ariles2_namevalue2_ws)"
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION share/${PROJECT_NAME}/
)
# ---
