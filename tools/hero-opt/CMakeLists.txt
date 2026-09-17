get_property(dialect_libs GLOBAL PROPERTY MLIR_DIALECT_LIBS)
get_property(conversion_libs GLOBAL PROPERTY MLIR_CONVERSION_LIBS)

add_llvm_executable(hero-opt hero-opt.cpp)
llvm_update_compile_flags(hero-opt)
target_link_libraries(hero-opt PRIVATE
  ${dialect_libs}
  ${conversion_libs}
  HeroDialect
  MLIROptLib
)

mlir_check_all_link_libraries(hero-opt)