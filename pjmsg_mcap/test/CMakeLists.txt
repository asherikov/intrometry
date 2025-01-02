find_package(GTest REQUIRED)

foreach(TEST_NAME combo intrometry)
    add_executable(test_${TEST_NAME} ${TEST_NAME}.cpp)
    target_link_libraries(test_${TEST_NAME} ${PROJECT_NAME}
        GTest::GTest
        thread_supervisor::thread_supervisor
    )
    add_test(test_${TEST_NAME} test_${TEST_NAME})
endforeach()
