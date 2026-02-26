add_executable(suru-bc src/main.cpp)

target_link_libraries(suru-bc PRIVATE libsuru-vm)
target_compile_features(suru-bc PRIVATE cxx_std_23)

if(BUILD_TESTING)
    add_test(NAME suru_bc_upvalue_test COMMAND suru-bc ${CMAKE_SOURCE_DIR}/tests/upvalue.sura)
endif()

