cmake_minimum_required(VERSION 3.8)
project(moebius)

add_executable(compiler main.cpp)
target_compile_features(compiler PUBLIC cxx_std_14)
