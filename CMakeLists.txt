cmake_minimum_required(VERSION 3.14)
project(ecewo-static VERSION 0.2.0 LANGUAGES C)

if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

if(PROJECT_IS_TOP_LEVEL)
  set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

option(ECEWO_STATIC_BUILD_SHARED "Build ecewo-static as a shared library" OFF)

if (PROJECT_IS_TOP_LEVEL AND NOT TARGET ecewo::ecewo)
  include(FetchContent)

  FetchContent_Declare(
    ecewo
    GIT_REPOSITORY https://github.com/ecewo/ecewo.git
    GIT_TAG v4.0.0
    GIT_SHALLOW TRUE
  )

  FetchContent_MakeAvailable(ecewo)
endif()

if(ECEWO_STATIC_BUILD_SHARED)
  add_library(ecewo-static SHARED src/ecewo-static.c)
else()
  add_library(ecewo-static STATIC src/ecewo-static.c)
endif()

add_library(ecewo::static ALIAS ecewo-static)

include(GenerateExportHeader)
generate_export_header(ecewo-static
  BASE_NAME ECEWO_STATIC
  EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/ecewo-static-export.h"
)

target_include_directories(ecewo-static PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
  $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

set_target_properties(ecewo-static PROPERTIES
  OUTPUT_NAME ecewo-static
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR}
  EXPORT_NAME static
  C_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
  POSITION_INDEPENDENT_CODE ON
)

if (PROJECT_IS_TOP_LEVEL AND TARGET ecewo::ecewo)
  ecewo_add(
    fs@dev
    mock@dev
  )
endif()

target_link_libraries(ecewo-static PUBLIC
  ecewo::ecewo
  ecewo::fs
)

if (PROJECT_IS_TOP_LEVEL AND TARGET ecewo::ecewo)
  enable_testing()

  add_executable(ecewo_static_test
    tests/test.c
  )

  target_include_directories(ecewo_static_test
    PRIVATE
      ${CMAKE_CURRENT_SOURCE_DIR}/src
      ${CMAKE_CURRENT_SOURCE_DIR}/tests
  )

  target_link_libraries(ecewo_static_test
    PRIVATE
      ecewo::ecewo
      ecewo::fs
      ecewo::static
      ecewo::mock
  )

  if (CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(ecewo_static_test PRIVATE
      -Wall
      -Wextra
      -Werror
      -Wstrict-prototypes
      -Wincompatible-pointer-types
      -Wno-unused-parameter
    )
  endif()

  add_test(
    NAME ecewo_static_all
    COMMAND ecewo_static_test
  )
endif()
