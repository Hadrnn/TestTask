cmake_minimum_required(VERSION 3.10)
project(TestTask)

include_directories(include)

set(CMAKE_CXX_STANDARD 20)

set(SOURCES 
    src/Main.cpp
    src/TextFS.cpp
)

set(HEADERS
    include/TestTask.h
    include/TextFS.h
)

add_executable(TestTask ${SOURCES} ${HEADERS})
