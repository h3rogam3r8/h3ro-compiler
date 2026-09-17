# add_mlir_dialect_library comes from MLIR's cmake modules.
add_mlir_dialect_library(HeroDialect
  HeroDialect.cpp

  ADDITIONAL_HEADER_DIRS
  ${PROJECT_SOURCE_DIR}/include/hero/Dialect/Hero

  DEPENDS
  MLIRHeroOpsIncGen

  LINK_LIBS PUBLIC
  MLIRIR
  MLIRSupport
)