add_executable(wait_example wait_example.cpp)
target_link_libraries(wait_example Coco)
set_target_properties(wait_example PROPERTIES CXX_STANDARD 20)

add_executable(tcp_example tcp_example.cpp)
target_link_libraries(tcp_example Coco)
set_target_properties(tcp_example PROPERTIES CXX_STANDARD 20)

add_executable(mutex_example mutex_example.cpp)
target_link_libraries(mutex_example Coco)
set_target_properties(mutex_example PROPERTIES CXX_STANDARD 20)

add_executable(channel_example channel_example.cpp)
target_link_libraries(channel_example Coco)
set_target_properties(channel_example PROPERTIES CXX_STANDARD 20)

add_executable(condvar_example condvar_example.cpp)
target_link_libraries(condvar_example Coco)
set_target_properties(condvar_example PROPERTIES CXX_STANDARD 20)

add_executable(rwlock_example rwlock_example.cpp)
target_link_libraries(rwlock_example Coco)
set_target_properties(rwlock_example PROPERTIES CXX_STANDARD 20)

add_executable(latch_example latch_example.cpp)
target_link_libraries(latch_example Coco)
set_target_properties(latch_example PROPERTIES CXX_STANDARD 20)

add_executable(udp_example udp_example.cpp)
target_link_libraries(udp_example Coco)
set_target_properties(udp_example PROPERTIES CXX_STANDARD 20)

add_executable(block_on_example block_on_example.cpp)
target_link_libraries(block_on_example Coco)
set_target_properties(block_on_example PROPERTIES CXX_STANDARD 20)

add_executable(simple_server simple_server.cpp)
target_link_libraries(simple_server Coco)
set_target_properties(simple_server PROPERTIES CXX_STANDARD 20)

add_executable(heavy_task heavy_task.cpp)
target_link_libraries(heavy_task Coco)
set_target_properties(heavy_task PROPERTIES CXX_STANDARD 20)

add_executable(chain_example chain_example.cpp)
target_link_libraries(chain_example Coco)
set_target_properties(chain_example PROPERTIES CXX_STANDARD 20)

# add a target run all example
add_custom_target(run_example
  COMMAND wait_example
  COMMAND tcp_example
  COMMAND mutex_example
  COMMAND channel_example
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
)