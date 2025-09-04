function(badger_cc_test)
  cmake_parse_arguments(TEST "" "NAME" "SRCS;COPTS;DEFINES;DEPS" ${ARGN})
  set(_NAME "${TEST_NAME}")
  
  badger_add_executable(
    NAME     ${_NAME}
    SRCS     ${TEST_SRCS}
    COPTS    ${TEST_COPTS}
    DEFINES  ${TEST_DEFINES}
    DEPS     ${TEST_DEPS}
    INCLUDES ${BADGER_INCLUDE_DIRS} ${GTEST_INCLUDE_DIRS}
  )
  
  target_link_libraries(${_NAME} PRIVATE GTest::gtest GTest::gtest_main)
  add_test(NAME ${_NAME} COMMAND ${_NAME})
endfunction()

badger_cc_test(
  NAME 
    cleanable_test
  SRCS 
    util/cleanable_test.cc
  DEPS 
    badger_util
)

badger_cc_test(
  NAME 
    arena_test
  SRCS 
    memory/arena_test.cc
  DEPS 
    badger_memory
)