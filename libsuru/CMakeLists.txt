add_library(libsuru STATIC
    src/parse.cpp
    src/parser.cpp
    src/lexer.cpp
    src/dump.cpp
    src/semantic.cpp
    src/compiler.cpp
)

target_include_directories(libsuru
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(libsuru PUBLIC libsuru-ir)
target_compile_features(libsuru PUBLIC cxx_std_23)

if(BUILD_TESTING)
    add_executable(front_smoke_test tests/front_smoke_test.cpp)
    target_link_libraries(front_smoke_test PRIVATE libsuru GTest::gtest_main)
    add_test(NAME front_smoke_test COMMAND front_smoke_test)
    add_executable(compiler_test tests/compiler_test.cpp)
    target_link_libraries(compiler_test PRIVATE libsuru libsuru-vm GTest::gtest_main)
    add_test(NAME compiler_test COMMAND compiler_test)
endif()
