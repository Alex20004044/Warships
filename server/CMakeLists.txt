cmake_minimum_required(VERSION 3.24)
project(server C)

set(CMAKE_C_STANDARD 11)

add_executable(server ../random_dev.h ../random_dev.c ../warships_field.h ../warships_field.c ../utils.h ../utils.c client_input_router.c server_main.c)
