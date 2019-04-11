cmake_minimum_required(VERSION 3.13)
include_directories(/usr/include/mpi/)
project(untitled)

set(CMAKE_CXX_STANDARD 14)

include_directories(.)

add_executable(untitled
        config.h
        main.c)
