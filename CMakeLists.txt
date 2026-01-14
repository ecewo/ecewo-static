cmake_minimum_required(VERSION 3.14)
project(ecewo-static VERSION 0.1.0 LANGUAGES C)

add_library(ecewo-static
  src/ecewo-static.c
)

add_library(ecewo::static ALIAS ecewo-static)

target_include_directories(ecewo-static PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)

if (PROJECT_IS_TOP_LEVEL AND NOT TARGET ecewo::ecewo)
  include(FetchContent)

  FetchContent_Declare(
    ecewo
    GIT_REPOSITORY https://github.com/savashn/ecewo.git
    GIT_TAG        dev
    GIT_SHALLOW    TRUE
  )

  FetchContent_MakeAvailable(ecewo)
endif()

if (PROJECT_IS_TOP_LEVEL AND TARGET ecewo::ecewo)
  ecewo_plugins(
    fs
    mock
  )

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

target_link_libraries(ecewo-static PUBLIC
  ecewo::ecewo
  ecewo::fs
)
