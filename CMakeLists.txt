cmake_minimum_required(VERSION 3.16)
project(OSfetch)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(osfetch
    src/main.cpp
    src/system.cpp
    src/render.cpp
    src/logo.cpp
)

target_include_directories(osfetch PRIVATE src)
