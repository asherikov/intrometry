/**
    @file
    @author  Alexander Sherikov
    @copyright 2025 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#pragma once

namespace  // NOLINT
{
    namespace intrometry_private::pjmsg_mcap
    {
        template <class t_Message>
        class Message
        {
        };


        template <>
        class Message<plotjuggler_msgs::msg::StatisticsNames>
        {
        public:
            inline static const char *const type = "plotjuggler_msgs/msg/StatisticsNames";  // NOLINT

            inline static const char *const schema =  // NOLINT
                    R"SCHEMA(
# header
std_msgs/Header header

# Statistics names
string[] names
uint32 names_version #This is increased each time names change

================================================================================
MSG: std_msgs/Header
# Standard metadata for higher-level stamped data types.
# This is generally used to communicate timestamped data
# in a particular coordinate frame.

# Two-integer timestamp that is expressed as seconds and nanoseconds.
builtin_interfaces/Time stamp

# Transform frame with which this data is associated.
string frame_id

================================================================================
MSG: builtin_interfaces/Time
# This message communicates ROS Time defined here:
# https://design.ros2.org/articles/clock_and_time.html

# The seconds component, valid over all int32 values.
int32 sec

# The nanoseconds component, valid in the range [0, 10e9).
uint32 nanosec
)SCHEMA";
        };


        template <>
        class Message<plotjuggler_msgs::msg::StatisticsValues>
        {
        public:
            inline static const char *const type = "plotjuggler_msgs/msg/StatisticsValues";  // NOLINT

            inline static const char *const schema =  // NOLINT
                    R"SCHEMA(
# header
std_msgs/Header header

# Statistics
float64[] values
uint32 names_version # The values vector corresponds to the name vector with the same name

================================================================================
MSG: std_msgs/Header
# Standard metadata for higher-level stamped data types.
# This is generally used to communicate timestamped data
# in a particular coordinate frame.

# Two-integer timestamp that is expressed as seconds and nanoseconds.
builtin_interfaces/Time stamp

# Transform frame with which this data is associated.
string frame_id

================================================================================
MSG: builtin_interfaces/Time
# This message communicates ROS Time defined here:
# https://design.ros2.org/articles/clock_and_time.html

# The seconds component, valid over all int32 values.
int32 sec

# The nanoseconds component, valid in the range [0, 10e9).
uint32 nanosec
)SCHEMA";
        };
    }  // namespace intrometry_private::pjmsg_mcap
}  // namespace
