add_executable(wait_example wait_example.cpp)
target_link_libraries(wait_example coco)
set_target_properties(wait_example PROPERTIES CXX_STANDARD 20)

add_executable(tcp_example tcp_example.cpp)
target_link_libraries(tcp_example coco)
set_target_properties(tcp_example PROPERTIES CXX_STANDARD 20)
