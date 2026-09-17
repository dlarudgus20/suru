add_executable(suru src/main.cpp)

target_link_libraries(suru PRIVATE libsuru libsuru-vm libsuru-ir)
target_compile_features(suru PRIVATE cxx_std_23)

if(BUILD_TESTING)
    add_executable(cli_smoke_test tests/cli_smoke_test.cpp)
    target_link_libraries(cli_smoke_test PRIVATE libsuru GTest::gtest_main)
    add_test(NAME cli_smoke_test COMMAND cli_smoke_test)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME source_cli_test
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/source_cli_test.py $<TARGET_FILE:suru>)
        add_test(NAME bytecode_cli_regression_test
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/bytecode_cli_test.py $<TARGET_FILE:suru>)
    endif()
    add_test(NAME suru_bytecode_vararg_test COMMAND suru ${CMAKE_SOURCE_DIR}/tests/vararg.sura)
    set_tests_properties(suru_bytecode_vararg_test PROPERTIES PASS_REGULAR_EXPRESSION "99.*20.*30")
    add_test(NAME suru_bytecode_emit_vararg_test COMMAND suru --sbc -o ${CMAKE_BINARY_DIR}/tests_vararg.sbc ${CMAKE_SOURCE_DIR}/tests/vararg.sura)
    set_tests_properties(suru_bytecode_emit_vararg_test PROPERTIES FIXTURES_SETUP suru_bytecode_vararg_file)
    add_test(NAME suru_bytecode_run_vararg_test COMMAND suru ${CMAKE_BINARY_DIR}/tests_vararg.sbc)
    set_tests_properties(suru_bytecode_run_vararg_test PROPERTIES FIXTURES_REQUIRED suru_bytecode_vararg_file PASS_REGULAR_EXPRESSION "99.*20.*30")
    add_test(NAME suru_bytecode_upvalue_test COMMAND suru ${CMAKE_SOURCE_DIR}/tests/upvalue.sura)
    add_test(NAME suru_bytecode_div_test COMMAND suru ${CMAKE_SOURCE_DIR}/tests/div.sura)
    add_test(NAME suru_bytecode_concat_len_test COMMAND suru ${CMAKE_SOURCE_DIR}/tests/concat_len.sura)
    add_test(NAME suru_bytecode_array_test COMMAND suru ${CMAKE_SOURCE_DIR}/tests/array.sura)
    add_test(NAME suru_bytecode_immediate_test COMMAND suru ${CMAKE_SOURCE_DIR}/tests/immediate.sura)
    add_test(NAME suru_bytecode_global_index_test COMMAND suru ${CMAKE_SOURCE_DIR}/tests/global_index.sura)
    add_test(
        NAME suru_bytecode_emit_sbc_test
        COMMAND suru --sbc -o ${CMAKE_BINARY_DIR}/tests_div.sbc ${CMAKE_SOURCE_DIR}/tests/div.sura
    )
    set_tests_properties(suru_bytecode_emit_sbc_test PROPERTIES FIXTURES_SETUP suru_bytecode_sbc_file)
    add_test(NAME suru_bytecode_run_sbc_test COMMAND suru ${CMAKE_BINARY_DIR}/tests_div.sbc)
    set_tests_properties(suru_bytecode_run_sbc_test PROPERTIES FIXTURES_REQUIRED suru_bytecode_sbc_file)
endif()
