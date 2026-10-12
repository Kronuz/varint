cmake_minimum_required(VERSION 3.14)
project(varint LANGUAGES CXX)

# --- Library target ---
# Header-only: varint.hh is the whole implementation. The target exists for
# the include path and the C++ standard requirement.
add_library(varint INTERFACE)
target_include_directories(varint INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(varint INTERFACE cxx_std_17)

# Alias so consumers can link `varint::varint` whether they add this via
# add_subdirectory or FetchContent.
add_library(varint::varint ALIAS varint)

# Only build the test and demo when this repo is the top-level project, so
# consumers (e.g. via FetchContent) don't build them.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  enable_testing()

  add_executable(varint_test test/test.cc)
  target_link_libraries(varint_test PRIVATE varint)
  target_compile_definitions(varint_test PRIVATE VARINT_FIXTURES="${CMAKE_CURRENT_SOURCE_DIR}/test/fixtures")
  add_test(NAME varint COMMAND varint_test)

  # libFuzzer target (Clang): cmake -DVARINT_FUZZ=ON -DCMAKE_CXX_COMPILER=clang++
  option(VARINT_FUZZ "Build the libFuzzer target" OFF)
  if(VARINT_FUZZ)
    add_executable(varint_fuzz fuzz/fuzz_decode.cc)
    target_link_libraries(varint_fuzz PRIVATE varint)
    target_compile_options(varint_fuzz PRIVATE -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
    target_link_options(varint_fuzz PRIVATE -fsanitize=fuzzer,address,undefined)
  endif()

  # A runnable tour of the library (not a test): build & run ./build/varint_demo
  add_executable(varint_demo examples/demo.cc)
  target_link_libraries(varint_demo PRIVATE varint)
endif()
