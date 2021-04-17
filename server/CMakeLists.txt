cmake_minimum_required(VERSION 3.17)
project(tic-tac-toe-server C)

set(CMAKE_C_STANDARD 11)

add_executable(server server.c)
target_compile_options(server PRIVATE -Wpedantic -Wall -Wextra)