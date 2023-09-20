cmake_minimum_required(VERSION 3.0.0)

project(net)
aux_source_directory(${NET} NET_SRC)
add_executable(net ${NET_SRC})
