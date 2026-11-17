cmake_minimum_required(VERSION 3.16)
project(orbi_test C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(orbi_test
    test_main.c
    ../components/pet_core/pet_state.c
    ../components/pet_core/tick_scheduler.c
    ../components/storage/storage_record.c
    ../components/input/chsc6x_parse.c
)

target_include_directories(orbi_test PRIVATE
    ../components/pet_core
    ../components/storage
    ../components/input
)

target_compile_options(orbi_test PRIVATE -Wall -Wextra -Wconversion)

