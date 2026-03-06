cmake_minimum_required(VERSION 3.24)
project(longhorn_library_2024)

set(CMAKE_CXX_STANDARD 17)

file(GLOB_RECURSE SOURCES LIST_DIRECTORIES true *.c *.h *.cpp)
set(SOURCES ${SOURCES})

add_library(longhorn_library_2024 ${SOURCES})
