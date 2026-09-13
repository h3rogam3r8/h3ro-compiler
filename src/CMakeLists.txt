add_library(hero_frontend
  Lexer.cpp
  Parser.cpp
  ASTPrinter.cpp
  SourceFile.cpp
  Diagnostics.cpp
  Sema.cpp
)

target_include_directories(hero_frontend
  PUBLIC ${PROJECT_SOURCE_DIR}/include
)

# New change for linking libraries

target_link_libraries(hero_frontend PRIVATE hero_warnings)