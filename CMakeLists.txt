cmake_minimum_required(VERSION 3.17)
project(cppref)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(${PROJECT_NAME}
        src/main.cpp)

target_include_directories(${PROJECT_NAME}
        INTERFACE
        include)
