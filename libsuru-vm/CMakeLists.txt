add_library(libsuru-vm STATIC
    src/lib.cpp
    src/object.cpp
    src/vm.cpp
    src/vm_exec.cpp
)

target_include_directories(libsuru-vm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(libsuru-vm PUBLIC libsuru-ir)

target_compile_features(libsuru-vm PUBLIC cxx_std_23)

if(BUILD_TESTING)
    add_executable(vm_smoke_test tests/vm_smoke_test.cpp)
    target_link_libraries(vm_smoke_test PRIVATE libsuru-vm GTest::gtest_main)
    add_test(NAME vm_smoke_test COMMAND vm_smoke_test)
    add_executable(vm_vararg_test tests/vm_vararg_test.cpp)
    target_link_libraries(vm_vararg_test PRIVATE libsuru-vm GTest::gtest_main)
    add_test(NAME vm_vararg_test COMMAND vm_vararg_test)
endif()
