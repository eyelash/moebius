cmake_minimum_required(VERSION 3.8)
project(moebius)

add_executable(moebc main.cpp)
target_compile_features(moebc PUBLIC cxx_std_17)
target_compile_options(moebc PUBLIC $<$<CXX_COMPILER_ID:GNU>:-Wall>)
