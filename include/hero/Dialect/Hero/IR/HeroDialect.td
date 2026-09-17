// The hero dialect. TableGen generates the C++ boilerplate from this.

#ifndef HERO_DIALECT
#define HERO_DIALECT

include "mlir/IR/OpBase.td"

def Hero_Dialect : Dialect {
  let name = "hero";
  let summary = "Tensor graph level ops for the Hero language";
  let cppNamespace = "::mlir::hero";
}

class Hero_Op<string mnemonic, list<Trait> traits = []>
    : Op<Hero_Dialect, mnemonic, traits>;

#endif  // HERO_DIALECT