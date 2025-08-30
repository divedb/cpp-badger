# 
function(badger_cc_test)
  cmake_parse_arguments(BADGER_CC_TEST "" "NAME"
                        "SRCS;COPTS;DEFINES;DEPS" ${ARGN})

  set(_NAME "${BADGER_CC_TEST_NAME}")                     
  add_executable(${_NAME} "")
  target_sources(${_NAME} PRIVATE ${BADGER_CC_TEST_SRCS})
  target_include_directories(
    ${_NAME}
    PRIVATE 
      ${BADGER_INCLUDE_DIRS}
      ${GTEST_INCLUDE_DIRS}
  )

  target_compile_definitions(${_NAME} PUBLIC ${BADGER_CC_TEST_DEFINES})
  target_compile_options(${_NAME} PRIVATE ${BADGER_CC_TEST_COPTS})

  target_link_libraries(
    ${_NAME}
    PRIVATE
      ${BADGER_CC_TEST_DEPS}
      GTest::gtest
      GTest::gtest_main
  )

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
