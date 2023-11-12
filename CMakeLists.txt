cmake_minimum_required(VERSION 3.8)
project(moebius)

add_executable(moebius main.cpp)
target_compile_features(moebius PUBLIC cxx_std_17)
target_compile_options(moebius PUBLIC $<$<CXX_COMPILER_ID:GNU>:-Wall>)
