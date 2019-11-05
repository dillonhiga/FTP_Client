cmake_minimum_required(VERSION 3.13)
project(a4_m4n0b_t3x0b C)

set(CMAKE_C_STANDARD 11)

include_directories(.)

add_executable(a4_m4n0b_t3x0b
        Ftp.c
        Ftp.h
        dir.c
        dir.h
        usage.c
        usage.h)
