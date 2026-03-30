cmake_minimum_required(VERSION 3.16)
project(microcodec C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wcast-align
        -Wstrict-prototypes
        -Wmissing-prototypes
        -Wconversion
        -Werror
    )
endif()

add_library(microcodec STATIC
    src/microcodec.c
    src/mc_rle.c
    src/mc_varint.c
    src/mc_delta.c
    src/mc_lzss.c
    src/mc_huff.c
)

target_include_directories(microcodec PUBLIC include)

add_library(microtest INTERFACE)
target_include_directories(microtest INTERFACE tests)

option(MICROCODEC_BUILD_TESTS "Build tests" ON)

if(MICROCODEC_BUILD_TESTS)
    enable_testing()

    function(microcodec_add_test name source)
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${source}")
            add_executable(${name} ${source})
            target_link_libraries(${name} PRIVATE microcodec microtest)
            add_test(NAME ${name} COMMAND ${name})
        endif()
    endfunction()

    microcodec_add_test(test_rle tests/test_rle.c)
    microcodec_add_test(test_varint tests/test_varint.c)
    microcodec_add_test(test_delta tests/test_delta.c)
    microcodec_add_test(test_lzss tests/test_lzss.c)
    microcodec_add_test(test_huff tests/test_huff.c)
    microcodec_add_test(test_integration tests/test_integration.c)
endif()
