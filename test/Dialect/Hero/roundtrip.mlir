// Parse the dialect and print it back out. If the assembly format in the
// td file is wrong this is where it shows up.
// RUN: hero-opt %s | FileCheck %s

// CHECK-LABEL: func.func @chain
func.func @chain(%a: tensor<2x3xf32>, %b: tensor<3x4xf32>,
                 %c: tensor<4x5xf32>) -> tensor<2x5xf32> {
  // CHECK: hero.matmul %arg0, %arg1 : tensor<2x3xf32>, tensor<3x4xf32> -> tensor<2x4xf32>
  %0 = hero.matmul %a, %b : tensor<2x3xf32>, tensor<3x4xf32> -> tensor<2x4xf32>

  // CHECK: hero.matmul %0, %arg2 : tensor<2x4xf32>, tensor<4x5xf32> -> tensor<2x5xf32>
  %1 = hero.matmul %0, %c : tensor<2x4xf32>, tensor<4x5xf32> -> tensor<2x5xf32>

  return %1 : tensor<2x5xf32>
}

// A dynamic dimension, which is how the symbolic B in a Hero
// tensor type is going to land once the lowering exists.
func.func @symbolic(%a: tensor<?x128xf16>, %b: tensor<128x64xf16>)
    -> tensor<?x64xf16> {
  // CHECK: hero.matmul %arg0, %arg1 : tensor<?x128xf16>, tensor<128x64xf16> -> tensor<?x64xf16>
  %0 = hero.matmul %a, %b : tensor<?x128xf16>, tensor<128x64xf16> -> tensor<?x64xf16>
  return %0 : tensor<?x64xf16>
}