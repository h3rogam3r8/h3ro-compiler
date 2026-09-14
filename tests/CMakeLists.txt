include(FetchContent)

# Downloads and builds googletest as part of configuring, so there's
# nothing to install by hand and the version is pinned right here.
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG        v1.15.2
)
FetchContent_MakeAvailable(googletest)

# Testing Block for modifications.
add_executable(hero_tests
  LexerTest.cpp
  ParserTest.cpp
  ASTPrinterTest.cpp
  SourceFileTest.cpp
  DiagnosticsTest.cpp
  LexerErrorTest.cpp
  ParserErrorTest.cpp
  TokenPrinterTest.cpp
  SemaTest.cpp
  SemaTypeTest.cpp
)
target_link_libraries(hero_tests PRIVATE hero_frontend hero_warnings GTest::gtest_main)

# Registers every TEST() in the binary with ctest individually.
include(GoogleTest)
gtest_discover_tests(hero_tests)

# Prefer GTest's syntax/look over CMake. 
add_custom_target(check
  COMMAND hero_tests --gtest_color=yes
  DEPENDS hero_tests
  USES_TERMINAL
)