cmake_minimum_required(VERSION 3.24)
project (client C)

set(CMAKE_C_STANDARD 11)

add_executable(client main.c ../utils.c ../utils.h ../warships_field.h ../warships_field.c)
