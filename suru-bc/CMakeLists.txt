add_executable(suru-bc src/main.cpp)

target_link_libraries(suru-bc PRIVATE libsuru-vm)
target_compile_features(suru-bc PRIVATE cxx_std_23)

