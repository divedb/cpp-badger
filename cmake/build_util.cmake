function(badger_setup_compiler_options target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
          -Wall
          -Wextra
          # -Wpedantic
          -Werror
          -fexceptions
          -frtti
        )
    elseif(MSVC)
        target_compile_options(${target} PRIVATE
          /W4
          /WX
          /EHsc
        )
    endif()
endfunction()

function(badger_setup_sanitizers target)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang" AND NOT MSVC)
        target_compile_options(${target} PRIVATE
          -fsanitize=address
          -fsanitize=undefined
          -fno-omit-frame-pointer
        )
        target_link_options(${target} PRIVATE
          -fsanitize=address
          -fsanitize=undefined
        )
    endif()
endfunction()

function(badger_add_executable)
    cmake_parse_arguments(BADGER
      "ENABLE_WARNINGS;ENABLE_SANITIZERS;INSTALL"
      "NAME;FOLDER;CXX_STANDARD"
      "SRCS;COPTS;DEFINES;DEPS;INCLUDES;LINK_OPTS"
      ${ARGN}
    )

    if(NOT BADGER_NAME)
      message(FATAL_ERROR "NAME is required for badger_add_executable")
    endif()

    add_executable(${BADGER_NAME} ${BADGER_SRCS})

    if(BADGER_INCLUDES)
      target_include_directories(${BADGER_NAME} PRIVATE ${BADGER_INCLUDES})
    endif()

    if(BADGER_DEFINES)
      target_compile_definitions(${BADGER_NAME} PRIVATE ${BADGER_DEFINES})
    endif()

    if(BADGER_COPTS)
      target_compile_options(${BADGER_NAME} PRIVATE ${BADGER_COPTS})
    endif()

    if(BADGER_DEPS)
      target_link_libraries(${BADGER_NAME} PRIVATE ${BADGER_DEPS})
    endif()

    if(BADGER_LINK_OPTS)
      target_link_options(${BADGER_NAME} PRIVATE ${BADGER_LINK_OPTS})
    endif()

    if(BADGER_ENABLE_WARNINGS)
      badger_setup_compiler_options(${BADGER_NAME})
    endif()

    if(BADGER_ENABLE_SANITIZERS)
      badger_setup_sanitizers(${BADGER_NAME})
    endif()

    set(cxx_standard 20)
    if(BADGER_CXX_STANDARD)
      set(cxx_standard ${BADGER_CXX_STANDARD})
    endif()

    set_target_properties(${BADGER_NAME} PROPERTIES
      CXX_STANDARD ${cxx_standard}
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
    )

    if(BADGER_FOLDER)
      set_target_properties(${BADGER_NAME} PROPERTIES FOLDER ${BADGER_FOLDER})
    endif()

    if(BADGER_INSTALL)
      install(TARGETS ${BADGER_NAME} DESTINATION bin)
    endif()
endfunction()