add_executable(suru src/main.cpp)

target_link_libraries(suru PRIVATE libsuru)
target_compile_features(suru PRIVATE cxx_std_23)

if(BUILD_TESTING)
    add_executable(cli_smoke_test tests/cli_smoke_test.cpp)
    target_link_libraries(cli_smoke_test PRIVATE libsuru GTest::gtest_main)
    add_test(NAME cli_smoke_test COMMAND cli_smoke_test)
endif()
