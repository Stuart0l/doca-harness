cmake_minimum_required(VERSION 3.14)

project(doca-harness)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-declarations")

add_subdirectory(src)
