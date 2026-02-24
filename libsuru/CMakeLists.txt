add_library(libsuru STATIC
    src/parse.cpp
    src/parser.cpp
    src/lexer.cpp
    src/dump.cpp
)

target_include_directories(libsuru
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(libsuru PUBLIC cxx_std_23)

if(BUILD_TESTING)
    add_executable(front_smoke_test tests/front_smoke_test.cpp)
    target_link_libraries(front_smoke_test PRIVATE libsuru)
    add_test(NAME front_smoke_test COMMAND front_smoke_test)
endif()
