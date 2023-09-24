cmake_minimum_required(VERSION 3.20)
project(Coco)

set(CMAKE_CXX_STANDARD 20)

find_library(LIBURING
  NAMES liburing.a
  REQUIRED
  HINTS "/usr/lib"
)

if(NOT LIBURING)
  message(FATAL_ERROR "liburing not found")
else()
  message(STATUS "liburing: ${LIBURING} found")
endif()

include(cmake/add_external.cmake)

# address santitlizer
if(CMAKE_BUILD_TYPE MATCHES Debug)
  message(STATUS "Build with address sanitizer")
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address -fno-omit-frame-pointer)
endif()

# target coco
add_library(Coco STATIC
  src/uring.cpp
  src/mt_executor.cpp
  src/inl_executor.cpp
  src/timer.cpp
  src/sys/socket_addr.cpp
)
set_target_properties(Coco PROPERTIES CXX_STANDARD 20)
target_include_directories(Coco PUBLIC include)
target_link_libraries(Coco PUBLIC ${LIBURING})
target_precompile_headers(Coco
  PUBLIC
  <algorithm>
  <array>
  <atomic>
  <cassert>
  <cstddef>
  <cstdint>
  <cstring>
  <exception>
  <span>
  <string_view>
  <system_error>
  <utility>
  <vector>
  <new>
  <format>
)

option(COCO_BUILD_EXAMPLE "Build example" OFF)
option(COCO_BUILD_TEST "Build test" OFF)

add_subdirectory(example)
add_subdirectory(test)
