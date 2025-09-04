set(UTIL_SOURCE_FILES
  util/cleanable.cc
  util/slice.cc
)

badger_add_library(
  NAME badger_util
  SRCS ${UTIL_SOURCE_FILES}
  INCLUDES ${BADGER_INCLUDE_DIRS}
  COPTS ${BADGER_CXX_FLAGS}
  DEPS absl::strings
  ENABLE_WARNINGS
)

SET(MEMORY_SOURCE_FILES
  memory/arena.cc
)

badger_add_library(
  NAME badger_memory
  SRCS ${MEMORY_SOURCE_FILES}
  INCLUDES ${BADGER_INCLUDE_DIRS} ${MIMALLOC_INCLUDE_DIRS}
  COPTS ${BADGER_CXX_FLAGS}
  DEPS mimalloc
  ENABLE_WARNINGS
  ENABLE_DEBUG
)
