# C++17 rather than 20 because that's what LLVM itself is built with

cmake_minimum_required(VERSION 3.20)
# C is in there because LLVM's HandleLLVMOptions checks C compiler flags
project(hero LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Writes build/compile_commands.json, which is what editors and clang-tidy
# read to know the exact flags each file is compiled with.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HERO_BUILD_TESTS "Build the unit tests" ON)
option(HERO_ENABLE_MLIR "Build the MLIR dialect and hero-opt" OFF)
option(HERO_WERROR "Turn warnings into errors" OFF)

# One place for the warning flags instead of repeating them in three
# CMakeLists. INTERFACE means it has no sources of its own, it just hands
# flags to whatever links it. Only applies to my targets, so googletest
# can warn all it likes without failing the build.
add_library(hero_warnings INTERFACE)
target_compile_options(hero_warnings INTERFACE -Wall -Wextra)
if(HERO_WERROR)
  target_compile_options(hero_warnings INTERFACE -Werror)
endif()

# Has to come before anything that calls add_test, including the MLIR
# tests below, or ctest never sees them.
if(HERO_BUILD_TESTS)
  enable_testing()
endif()

add_subdirectory(src)
add_subdirectory(tools/heroc)

# MLIR is off by default so the frontend still builds on a machine that
# hasn't got it. Turn it on with -DHERO_ENABLE_MLIR=ON.
if(HERO_ENABLE_MLIR)
  find_package(MLIR REQUIRED CONFIG)
  message(STATUS "Found MLIR ${MLIR_PACKAGE_VERSION} at ${MLIR_DIR}")

  # These bring in the add_mlir_dialect and add_mlir_dialect_library
  # helpers plus the tablegen machinery.
  list(APPEND CMAKE_MODULE_PATH "${MLIR_CMAKE_DIR}")
  list(APPEND CMAKE_MODULE_PATH "${LLVM_CMAKE_DIR}")
  include(TableGen)
  include(AddLLVM)
  include(AddMLIR)
  include(HandleLLVMOptions)

  include_directories(${LLVM_INCLUDE_DIRS})
  include_directories(${MLIR_INCLUDE_DIRS})
  include_directories(${PROJECT_SOURCE_DIR}/include)
  include_directories(${PROJECT_BINARY_DIR}/include)
  link_directories(${LLVM_BUILD_LIBRARY_DIR})
  add_definitions(${LLVM_DEFINITIONS})

  add_subdirectory(include/hero/Dialect/Hero/IR)
  add_subdirectory(lib/Dialect/Hero/IR)
  add_subdirectory(tools/hero-opt)

  if(HERO_BUILD_TESTS)
    add_subdirectory(test)
  endif()
endif()

if(HERO_BUILD_TESTS)
  add_subdirectory(tests)
endif()