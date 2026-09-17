#ifndef HERO_DIALECT_HERO_IR_HEROOPS_H
#define HERO_DIALECT_HERO_IR_HEROOPS_H

#include "hero/Dialect/Hero/IR/HeroDialect.h"

#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/OpDefinition.h"
// Don't remove, breaks things
#include "mlir/IR/OpImplementation.h"

// Older LLVM specific fix
#include "mlir/Interfaces/SideEffectInterfaces.h"

#define GET_OP_CLASSES
#include "hero/Dialect/Hero/IR/HeroOps.h.inc"

#endif  // HERO_DIALECT_HERO_IR_HEROOPS_H