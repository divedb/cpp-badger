set(UTIL_SOURCE_FILES
  util/cleanable.cc
)

add_library(badger_util ${UTIL_SOURCE_FILES})
target_include_directories(badger_util PRIVATE ${BADGER_INCLUDE_DIRS})
target_compile_options(badger_util PRIVATE ${BADGER_CXX_FLAGS})