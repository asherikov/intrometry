find_package(GTest REQUIRED)
find_package(thread_supervisor REQUIRED)

set(TEST_BACKEND pjmsg_mcap)
foreach(TEST_NAME combo intrometry)
    find_package(intrometry_${TEST_BACKEND} REQUIRED)

    add_executable(test_${TEST_BACKEND}_${TEST_NAME} ${TEST_BACKEND}_${TEST_NAME}.cpp)
    target_link_libraries(test_${TEST_BACKEND}_${TEST_NAME}
        intrometry::${TEST_BACKEND}
        GTest::GTest
    )
    add_test(test_${TEST_BACKEND}_${TEST_NAME} test_${TEST_BACKEND}_${TEST_NAME})
endforeach()

set(TEST_BACKEND pjmsg_topic)
foreach(TEST_NAME combo intrometry)
    find_package(rclcpp REQUIRED)
    find_package(plotjuggler_msgs REQUIRED)
    find_package(intrometry_${TEST_BACKEND} REQUIRED)

    add_executable(test_${TEST_BACKEND}_${TEST_NAME} ${TEST_BACKEND}_${TEST_NAME}.cpp)
    target_link_libraries(test_${TEST_BACKEND}_${TEST_NAME}
        intrometry::${TEST_BACKEND}
        GTest::GTest
        thread_supervisor::thread_supervisor
        rclcpp::rclcpp
        plotjuggler_msgs::plotjuggler_msgs__rosidl_typesupport_cpp
    )
    add_test(test_${TEST_BACKEND}_${TEST_NAME} test_${TEST_BACKEND}_${TEST_NAME})
endforeach()


foreach(TEST_NAME sink_base)
    add_executable(test_${TEST_NAME} ${TEST_NAME}.cpp)
    target_link_libraries(test_${TEST_NAME}
        intrometry::pjmsg_topic
        intrometry::pjmsg_mcap
        GTest::GTest
    )
    add_test(test_${TEST_NAME} test_${TEST_NAME})
endforeach()
