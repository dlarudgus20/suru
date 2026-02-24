add_library(libsuru-vm STATIC
    src/vm.cpp
)

target_include_directories(libsuru-vm
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(libsuru-vm PUBLIC cxx_std_23)

if(BUILD_TESTING)
    add_executable(vm_smoke_test tests/vm_smoke_test.cpp)
    target_link_libraries(vm_smoke_test PRIVATE libsuru-vm)
    add_test(NAME vm_smoke_test COMMAND vm_smoke_test)
endif()
