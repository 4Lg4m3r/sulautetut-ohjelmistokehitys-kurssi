
cmake_minimum_required(VERSION 3.14)

project(TimeParserTests)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

enable_testing()

find_package(GTest REQUIRED)

add_executable(TimeParserTest
    TimeParserTest.cpp
    ../timeparser.c
)

target_include_directories(TimeParserTest PRIVATE
    ..
)

target_link_libraries(TimeParserTest
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(TimeParserTest)
