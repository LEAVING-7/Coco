cmake_minimum_required(VERSION 3.20)
project(coro)

set(CMAKE_CXX_STANDARD 20)

find_library(LIBURING
  NAMES liburing.a
  REQUIRED
  PATHS "/usr/lib"
)
message(STATUS "liburing: ${LIBURING} found")

include(cmake/add_external.cmake)


# address santitlizer
add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
add_link_options(-fsanitize=address -fno-omit-frame-pointer)

# target coco
add_library(coco STATIC
  src/uring.cpp
  src/mt_executor.cpp
  src/timer.cpp
)
set_target_properties(coco PROPERTIES CXX_STANDARD 20)
target_include_directories(coco PUBLIC include)
target_link_libraries(coco PUBLIC ${LIBURING})

add_subdirectory(example)
add_subdirectory(test)