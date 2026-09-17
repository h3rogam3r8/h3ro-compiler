#include "hero/Dialect/Hero/IR/HeroOps.h"
#include "mlir/IR/Builders.h"

using namespace mlir;
using namespace mlir::hero;

#include "hero/Dialect/Hero/IR/HeroOpsDialect.cpp.inc"

void HeroDialect::initialize() {
  addOperations<
#define GET_OP_LIST
#include "hero/Dialect/Hero/IR/HeroOps.cpp.inc"
      >();
}

#define GET_OP_CLASSES
#include "hero/Dialect/Hero/IR/HeroOps.cpp.inc"