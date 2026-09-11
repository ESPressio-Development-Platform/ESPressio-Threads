cmake_minimum_required(VERSION 3.16)
project(ESPressioThreadContracts LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ESPRESSIO_DEPENDENCIES "${CMAKE_CURRENT_LIST_DIR}/../.." CACHE PATH "Coordinated repository checkouts")
find_package(Threads REQUIRED)
enable_testing()
foreach(test thread_lifecycle precision composition common_wake_and_deadlines)
    add_executable(test_${test} test_${test}.cpp)
    target_include_directories(test_${test} PRIVATE ../src
        "${ESPRESSIO_DEPENDENCIES}/ESPressio-System/src"
        "${ESPRESSIO_DEPENDENCIES}/ESPressio-Task/src"
        "${ESPRESSIO_DEPENDENCIES}/ESPressio-Task/tests"
        "${ESPRESSIO_DEPENDENCIES}/ESPressio-Timing/src")
    target_compile_options(test_${test} PRIVATE -Wall -Wextra -Werror -fno-rtti)
    target_link_libraries(test_${test} PRIVATE Threads::Threads)
    add_test(NAME ${test} COMMAND test_${test})
endforeach()
add_executable(test_timing_independence test_timing_independence.cpp)
target_include_directories(test_timing_independence PRIVATE ../src
    "${ESPRESSIO_DEPENDENCIES}/ESPressio-System/src"
    "${ESPRESSIO_DEPENDENCIES}/ESPressio-Task/src"
    "${ESPRESSIO_DEPENDENCIES}/ESPressio-Task/tests"
    "${ESPRESSIO_DEPENDENCIES}/ESPressio-Timing/src"
    "${ESPRESSIO_DEPENDENCIES}/ESPressio-Timing/tests"
    "${ESPRESSIO_DEPENDENCIES}/ESPressio-Timing/tests/stubs"
    "${ESPRESSIO_DEPENDENCIES}/ESPressio-Units/src"
    "${ESPRESSIO_DEPENDENCIES}/ESPressio-Observable/src")
target_compile_options(test_timing_independence PRIVATE -Wall -Wextra -Werror -fno-rtti)
target_link_libraries(test_timing_independence PRIVATE Threads::Threads)
add_test(NAME timing_independence COMMAND test_timing_independence)
