cmake_minimum_required(VERSION 3.0.0)

get_filename_component(ROOTDIR ".." ABSOLUTE)
set(SRCDIR ${ROOTDIR}/src)
set(COMMON ${SRCDIR}/common)
set(NET ${SRCDIR}/net)
set(PROC ${SRCDIR}/proc)

include_directories(${COMMON} ${NET} ${PROC})

if(CMAKE_HOST_WIN32)
add_definitions(-D_UNICODE -DUNICODE)
add_compile_options(/wd4996)
endif()

message(STATUS ${ROOTDIR})
message(STATUS ${SRCDIR})
message(STATUS ${COMMON})
message(STATUS ${NET})
message(STATUS ${PROC})

project(proc)
aux_source_directory(${COMMON} SRC)
aux_source_directory(${SRCDIR} SRC)
aux_source_directory(${PROC} SRC)
add_executable(proc ${SRC})

add_subdirectory(net)
