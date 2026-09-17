# MLIR style tests: run hero-opt over a .mlir file and match the output
# with FileCheck. Normally this would be driven by lit but gtest already works so
find_program(FILECHECK_EXE NAMES FileCheck FileCheck-18 FileCheck-19 FileCheck-20
             HINTS ${LLVM_TOOLS_BINARY_DIR})

if(NOT FILECHECK_EXE)
  message(STATUS "FileCheck not found, skipping the dialect tests")
  return()
endif()

message(STATUS "Using FileCheck at ${FILECHECK_EXE}")

file(GLOB_RECURSE mlir_tests CONFIGURE_DEPENDS
     ${CMAKE_CURRENT_SOURCE_DIR}/Dialect/*.mlir)

foreach(test_file ${mlir_tests})
  file(RELATIVE_PATH test_name ${CMAKE_CURRENT_SOURCE_DIR} ${test_file})

  add_test(
    NAME mlir/${test_name}
    COMMAND ${CMAKE_COMMAND}
            -DHERO_OPT=$<TARGET_FILE:hero-opt>
            -DFILECHECK=${FILECHECK_EXE}
            -DTEST_FILE=${test_file}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/RunMlirTest.cmake)
endforeach()