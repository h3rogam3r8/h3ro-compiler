# Pipes one .mlir file through hero-opt and into FileCheck. Kept in its
# own script because add_test can't do a shell pipeline directly.

execute_process(
  COMMAND ${HERO_OPT} ${TEST_FILE}
  OUTPUT_VARIABLE opt_output
  ERROR_VARIABLE opt_errors
  RESULT_VARIABLE opt_result)

if(NOT opt_result EQUAL 0)
  message(FATAL_ERROR "hero-opt failed on ${TEST_FILE}\n${opt_errors}")
endif()

get_filename_component(test_name ${TEST_FILE} NAME)
set(tmp_out "${CMAKE_CURRENT_BINARY_DIR}/${test_name}.out")
file(WRITE ${tmp_out} "${opt_output}")

execute_process(
  COMMAND ${FILECHECK} ${TEST_FILE}
  INPUT_FILE ${tmp_out}
  RESULT_VARIABLE check_result
  ERROR_VARIABLE check_errors)

if(NOT check_result EQUAL 0)
  message(FATAL_ERROR "FileCheck failed on ${TEST_FILE}\n${check_errors}")
endif()