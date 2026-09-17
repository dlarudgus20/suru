add_library(libsuru-ir STATIC
    src/assembler.cpp
    src/builder.cpp
    src/disassembler.cpp
    src/image.cpp
)

target_include_directories(libsuru-ir
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(libsuru-ir PUBLIC cxx_std_23)

if(BUILD_TESTING)
    add_executable(ir_test tests/ir_test.cpp)
    target_link_libraries(ir_test PRIVATE libsuru-ir GTest::gtest_main)
    add_test(NAME ir_test COMMAND ir_test)
endif()
