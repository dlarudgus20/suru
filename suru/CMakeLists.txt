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
    endif()
endif()
