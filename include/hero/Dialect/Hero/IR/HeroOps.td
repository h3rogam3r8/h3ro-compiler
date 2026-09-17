#ifndef HERO_OPS
#define HERO_OPS

include "hero/Dialect/Hero/IR/HeroDialect.td"
include "mlir/Interfaces/SideEffectInterfaces.td"

// Starting with one op just to prove the whole TableGen pipeline works.
def Hero_MatmulOp : Hero_Op<"matmul", [Pure]> {
  let summary = "Matrix multiply, [M, K] times [K, N]";

  let arguments = (ins AnyRankedTensor:$lhs, AnyRankedTensor:$rhs);
  let results = (outs AnyRankedTensor:$result);

  let assemblyFormat =
      "$lhs `,` $rhs attr-dict `:` type($lhs) `,` type($rhs) `->` type($result)";
}

#endif  // HERO_OPS