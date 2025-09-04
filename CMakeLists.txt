cmake_minimum_required(VERSION 3.20)
project(cpp-badger LANGUAGES CXX)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
include(build_util)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)
# absl ISO C++ does not support ‘__int128’
# set(BADGER_CXX_FLAGS ${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror -Wpedantic)
set(BADGER_CXX_FLAGS ${CMAKE_CXX_FLAGS})
set(BADGER_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(MIMALLOC_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mimalloc/include)

find_package(GTest REQUIRED)

add_subdirectory(src)
add_subdirectory(third_party/abseil)
add_subdirectory(third_party/mimalloc)

if(BUILD_TESTING)
  enable_testing()
  add_subdirectory(tests)
endif()

add_subdirectory(examples)
