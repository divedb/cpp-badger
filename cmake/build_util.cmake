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
      "ENABLE_DEBUG;ENABLE_WARNINGS;ENABLE_SANITIZERS;INSTALL"
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

    if (BADGER_ENABLE_DEBUG)
      add_compile_options(${BADGER_NAME} -g)
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

function(badger_add_library)
    cmake_parse_arguments(BADGER
      "ENABLE_WARNINGS;ENABLE_SANITIZERS;INSTALL;SHARED;STATIC"
      "NAME;FOLDER;CXX_STANDARD;VERSION;SOVERSION"
      "SRCS;COPTS;DEFINES;DEPS;INCLUDES;LINK_OPTS;PUBLIC_INCLUDES;PUBLIC_DEFINES;PUBLIC_DEPS"
      ${ARGN}
    )

    if(NOT BADGER_NAME)
      message(FATAL_ERROR "NAME is required for badger_add_library")
    endif()

    # Determine library type
    if(BADGER_SHARED AND BADGER_STATIC)
      message(FATAL_ERROR "Cannot specify both SHARED and STATIC for library ${BADGER_NAME}")
    endif()

    set(library_type)
    if(BADGER_SHARED)
      set(library_type SHARED)
    elseif(BADGER_STATIC)
      set(library_type STATIC)
    else()
      # Default to static if not specified.
      set(library_type STATIC)
    endif()

    # Create the library.
    add_library(${BADGER_NAME} ${library_type} ${BADGER_SRCS})

    # Set library version properties if provided.
    if(BADGER_VERSION AND BADGER_SOVERSION)
        set_target_properties(${BADGER_NAME} PROPERTIES
            VERSION ${BADGER_VERSION}
            SOVERSION ${BADGER_SOVERSION}
        )
    elseif(BADGER_VERSION)
        set_target_properties(${BADGER_NAME} PROPERTIES
            VERSION ${BADGER_VERSION}
        )
    elseif(BADGER_SOVERSION)
        set_target_properties(${BADGER_NAME} PROPERTIES
            SOVERSION ${BADGER_SOVERSION}
        )
    endif()

    # Private include directories
    if(BADGER_INCLUDES)
        target_include_directories(${BADGER_NAME} PRIVATE ${BADGER_INCLUDES})
    endif()

    # Public include directories
    if(BADGER_PUBLIC_INCLUDES)
      target_include_directories(${BADGER_NAME} PUBLIC ${BADGER_PUBLIC_INCLUDES})
    endif()

    # Private compile definitions
    if(BADGER_DEFINES)
      target_compile_definitions(${BADGER_NAME} PRIVATE ${BADGER_DEFINES})
    endif()

    # Public compile definitions
    if(BADGER_PUBLIC_DEFINES)
      target_compile_definitions(${BADGER_NAME} PUBLIC ${BADGER_PUBLIC_DEFINES})
    endif()

    # Compile options
    if(BADGER_COPTS)
      target_compile_options(${BADGER_NAME} PRIVATE ${BADGER_COPTS})
    endif()

    # Private dependencies
    if(BADGER_DEPS)
      target_link_libraries(${BADGER_NAME} PRIVATE ${BADGER_DEPS})
    endif()

    # Public dependencies
    if(BADGER_PUBLIC_DEPS)
      target_link_libraries(${BADGER_NAME} PUBLIC ${BADGER_PUBLIC_DEPS})
    endif()

    # Link options
    if(BADGER_LINK_OPTS)
      target_link_options(${BADGER_NAME} PRIVATE ${BADGER_LINK_OPTS})
    endif()

    # Compiler warnings and options
    if(BADGER_ENABLE_WARNINGS)
      badger_setup_compiler_options(${BADGER_NAME})
    endif()

    # Sanitizers
    if(BADGER_ENABLE_SANITIZERS)
      badger_setup_sanitizers(${BADGER_NAME})
    endif()

    # C++ standard
    set(cxx_standard 20)
    if(BADGER_CXX_STANDARD)
      set(cxx_standard ${BADGER_CXX_STANDARD})
    endif()

    set_target_properties(${BADGER_NAME} PROPERTIES
      CXX_STANDARD ${cxx_standard}
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
    )

    # Folder organization (for IDEs)
    if(BADGER_FOLDER)
      set_target_properties(${BADGER_NAME} PROPERTIES FOLDER ${BADGER_FOLDER})
    endif()

    # Installation
    if(BADGER_INSTALL)
      install(TARGETS ${BADGER_NAME}
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
      )
        
      # Install public headers if specified
      if(BADGER_PUBLIC_INCLUDES)
        foreach(include_dir ${BADGER_PUBLIC_INCLUDES})
          install(DIRECTORY ${include_dir} 
                  DESTINATION include
                  FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp"
          )
        endforeach()
      endif()
  endif()
endfunction()