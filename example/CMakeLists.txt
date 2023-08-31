add_executable(example example.cpp)
target_link_libraries(example coco)
set_target_properties(example PROPERTIES CXX_STANDARD 20)