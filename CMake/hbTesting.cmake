# Generate a test driver executable with the given name
function(hb_test_single c_file_name test_case_name target_link_libs target_compile_defs)
	set(TEST_CASE_NAME ${test_case_name})
	set(TEST_SRC_C ${c_file_name}.c)
	set(TEST_OUTPUT_C ${CMAKE_CURRENT_BINARY_DIR}/${c_file_name}_runner_single.c)
	configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tests/runner_single.c.in ${TEST_OUTPUT_C})
	
	set(TEST_SINGLE_TARGET ${c_file_name}_runner_single)
	
	add_executable(${TEST_SINGLE_TARGET} ${TEST_OUTPUT_C})
	target_include_directories(${TEST_SINGLE_TARGET} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests)
	target_link_libraries(${TEST_SINGLE_TARGET} ${target_link_libs})
	target_compile_definitions(${TEST_SINGLE_TARGET} PRIVATE ${target_compile_defs} -DAWS_UNSTABLE_TESTING_API)
endfunction()