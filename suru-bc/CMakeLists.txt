add_executable(suru-bc src/main.cpp)

target_link_libraries(suru-bc PRIVATE libsuru-ir libsuru-vm)
target_compile_features(suru-bc PRIVATE cxx_std_23)

if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME suru_bc_cli_regression_test
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/bytecode_cli_test.py $<TARGET_FILE:suru-bc>)
    endif()
    add_test(NAME suru_bc_vararg_test COMMAND suru-bc ${CMAKE_SOURCE_DIR}/tests/vararg.sura)
    set_tests_properties(suru_bc_vararg_test PROPERTIES PASS_REGULAR_EXPRESSION "99.*20.*30")
    add_test(NAME suru_bc_emit_vararg_test COMMAND suru-bc -o ${CMAKE_BINARY_DIR}/tests_vararg.sbc ${CMAKE_SOURCE_DIR}/tests/vararg.sura)
    set_tests_properties(suru_bc_emit_vararg_test PROPERTIES FIXTURES_SETUP suru_bc_vararg_file)
    add_test(NAME suru_bc_run_vararg_test COMMAND suru-bc ${CMAKE_BINARY_DIR}/tests_vararg.sbc)
    set_tests_properties(suru_bc_run_vararg_test PROPERTIES FIXTURES_REQUIRED suru_bc_vararg_file PASS_REGULAR_EXPRESSION "99.*20.*30")
    add_test(NAME suru_bc_upvalue_test COMMAND suru-bc ${CMAKE_SOURCE_DIR}/tests/upvalue.sura)
    add_test(NAME suru_bc_div_test COMMAND suru-bc ${CMAKE_SOURCE_DIR}/tests/div.sura)
    add_test(NAME suru_bc_concat_len_test COMMAND suru-bc ${CMAKE_SOURCE_DIR}/tests/concat_len.sura)
    add_test(NAME suru_bc_array_test COMMAND suru-bc ${CMAKE_SOURCE_DIR}/tests/array.sura)
    add_test(NAME suru_bc_immediate_test COMMAND suru-bc ${CMAKE_SOURCE_DIR}/tests/immediate.sura)
    add_test(NAME suru_bc_global_index_test COMMAND suru-bc ${CMAKE_SOURCE_DIR}/tests/global_index.sura)
    add_test(
        NAME suru_bc_emit_sbc_test
        COMMAND suru-bc -o ${CMAKE_BINARY_DIR}/tests_div.sbc ${CMAKE_SOURCE_DIR}/tests/div.sura
    )
    set_tests_properties(suru_bc_emit_sbc_test PROPERTIES FIXTURES_SETUP suru_bc_sbc_file)
    add_test(NAME suru_bc_run_sbc_test COMMAND suru-bc ${CMAKE_BINARY_DIR}/tests_div.sbc)
    set_tests_properties(suru_bc_run_sbc_test PROPERTIES FIXTURES_REQUIRED suru_bc_sbc_file)
endif()
