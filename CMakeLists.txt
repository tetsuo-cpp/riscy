cmake_minimum_required(VERSION 3.16)

project(riscy VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Options
option(RISCY_WARNINGS "Enable extra compiler warnings" ON)

add_library(riscy_lib
  src/ELFImage.cpp
  src/IR/IR.cpp
  src/RISCV/CFG.cpp
  src/RISCV/Decoder.cpp
  src/RISCV/Lifter.cpp
  src/RISCV/Printer.cpp
)

target_include_directories(riscy_lib PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/ELFIO
)

add_executable(riscy
  tools/riscy.cpp
)

target_link_libraries(riscy PRIVATE riscy_lib)

if (RISCY_WARNINGS)
  if (MSVC)
    target_compile_options(riscy PRIVATE /W4)
    target_compile_options(riscy_lib PRIVATE /W4)
  else()
    target_compile_options(riscy PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(riscy_lib PRIVATE -Wall -Wextra -Wpedantic)
  endif()
endif()

install(TARGETS riscy RUNTIME DESTINATION bin)

# Formatting target
find_program(CLANG_FORMAT NAMES clang-format)
if (CLANG_FORMAT)
  file(GLOB_RECURSE RISCY_FORMAT_SOURCES CONFIGURE_DEPENDS
       ${CMAKE_CURRENT_SOURCE_DIR}/src/*.h
       ${CMAKE_CURRENT_SOURCE_DIR}/src/*.hpp
       ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/tools/*.h
       ${CMAKE_CURRENT_SOURCE_DIR}/tools/*.hpp
       ${CMAKE_CURRENT_SOURCE_DIR}/tools/*.cpp
       ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.h
       ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.hpp
       ${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp)
  add_custom_target(format
    COMMAND ${CLANG_FORMAT} -i -style=file -fallback-style=LLVM ${RISCY_FORMAT_SOURCES}
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Running clang-format on source files")
else()
  add_custom_target(format
    COMMAND ${CMAKE_COMMAND} -E echo "clang-format not found. Install it to use the 'format' target."
    COMMAND ${CMAKE_COMMAND} -E false)
endif()

# Tests
include(CTest)
if (BUILD_TESTING)
  # Catch2
  add_subdirectory(third_party/Catch2)

  add_executable(decoder_tests tests/DecoderTests.cpp)
  target_link_libraries(decoder_tests PRIVATE riscy_lib Catch2::Catch2WithMain)
  target_include_directories(decoder_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  if (RISCY_WARNINGS)
    if (MSVC)
      target_compile_options(decoder_tests PRIVATE /W4)
    else()
      target_compile_options(decoder_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
  endif()
  add_test(NAME decoder_tests COMMAND decoder_tests)

  # CFG tests
  add_executable(cfg_tests tests/CFGTests.cpp)
  target_link_libraries(cfg_tests PRIVATE riscy_lib Catch2::Catch2WithMain)
  target_include_directories(cfg_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  if (RISCY_WARNINGS)
    if (MSVC)
      target_compile_options(cfg_tests PRIVATE /W4)
    else()
      target_compile_options(cfg_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
  endif()
  # Run Catch2 with -s to show std::cout prints from tests
  add_test(NAME cfg_tests COMMAND cfg_tests -s)

  # IR tests
  add_executable(ir_tests tests/IRTests.cpp)
  target_link_libraries(ir_tests PRIVATE riscy_lib Catch2::Catch2WithMain)
  target_include_directories(ir_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
  )
  if (RISCY_WARNINGS)
    if (MSVC)
      target_compile_options(ir_tests PRIVATE /W4)
    else()
      target_compile_options(ir_tests PRIVATE -Wall -Wextra -Wpedantic)
    endif()
  endif()
  add_test(NAME ir_tests COMMAND ir_tests -s)

  find_package(Python3 COMPONENTS Interpreter REQUIRED)
  add_test(NAME e2e_decode
    COMMAND Python3::Interpreter -m pytest ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_decode.py
  )
  set_tests_properties(e2e_decode PROPERTIES
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    ENVIRONMENT "RISCY_BUILD_DIR=${CMAKE_BINARY_DIR};PYTEST_ADDOPTS=-s"
  )
endif()
