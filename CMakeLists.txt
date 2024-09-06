cmake_minimum_required(VERSION 3.22)

project(library)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(include/print)
add_subdirectory(include/demangle)

add_subdirectory(tests)