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

# LLVM 21 moved registerAllDialects and registerAllPasses out of the
# headers and into libraries of their own. 
foreach(reg_lib MLIRRegisterAllDialects MLIRRegisterAllPasses
                MLIRRegisterAllExtensions)
  if(TARGET ${reg_lib})
    target_link_libraries(hero-opt PRIVATE ${reg_lib})
  endif()
endforeach()

mlir_check_all_link_libraries(hero-opt)