enable_testing()

add_external(googletest; https://ghproxy.com/https://github.com/google/googletest.git; v1.13.0)

add_executable(timer_test timer_test.cpp)
target_link_libraries(timer_test gtest_main Coco)

include(GoogleTest)
gtest_discover_tests(timer_test)