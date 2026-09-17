// hero-opt is the same shape as mlir-opt: read MLIR in, run passes over
// it, print MLIR out. Nothing of substance built yet.

#include "hero/Dialect/Hero/IR/HeroOps.h"
#include "mlir/IR/DialectRegistry.h"
#include "mlir/InitAllDialects.h"
#include "mlir/InitAllPasses.h"
#include "mlir/Tools/mlir-opt/MlirOptMain.h"

int main(int argc, char **argv) {
  mlir::registerAllPasses();

  mlir::DialectRegistry registry;
  mlir::registerAllDialects(registry);
  registry.insert<mlir::hero::HeroDialect>();

  return mlir::asMainReturnCode(
      mlir::MlirOptMain(argc, argv, "hero optimizer driver\n", registry));
}