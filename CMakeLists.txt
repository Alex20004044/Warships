cmake_minimum_required(VERSION 3.24)
project(Warships_6 C)

set(CMAKE_C_STANDARD 11)

add_library(Warships_6 random_dev.h utils.c warships_field.c)
