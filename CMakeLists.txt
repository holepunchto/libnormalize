cmake_minimum_required(VERSION 4.0)

find_package(cmake-fetch REQUIRED PATHS node_modules/cmake-fetch)
find_package(cmake-fuzz REQUIRED PATHS node_modules/cmake-fuzz)
find_package(cmake-prelude REQUIRED PATHS node_modules/cmake-prelude)
find_package(cmake-test REQUIRED PATHS node_modules/cmake-test)
find_package(cmake-ucd REQUIRED PATHS node_modules/cmake-ucd)

project(normalize LANGUAGES C VERSION 0.0.0)

fetch_package("github:holepunchto/libutf")

include(cmake/tables.cmake)

add_library_targets(
  normalize
  HEADERS
    include/normalize.h
    "${normalize_tables_header}"
  BASE_DIRS
    include
    "${CMAKE_CURRENT_BINARY_DIR}/include"
  FEATURES
    c_std_11
  SOURCES
    src/normalize.c
    "${normalize_tables_source}"
  LINK
    utf::headers
)

add_dependencies(normalize_headers normalize_tables)

add_test_subdirectory(test)

if(PROJECT_IS_TOP_LEVEL)
  set(BENCH OFF CACHE BOOL "Build the benchmarks")

  if(BENCH)
    add_subdirectory(bench)
  endif()
endif()
