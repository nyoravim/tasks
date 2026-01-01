cmake_minimum_required(VERSION 3.20)
project(tasks LANGUAGES C)

file(GLOB_RECURSE SRC "src/*.c")
add_executable(tasks ${SRC})

find_package(json-c REQUIRED)
find_package(libnyoravim REQUIRED)

target_link_libraries(
    tasks PRIVATE 
    pthread
    discord
    curl
    json-c::json-c
    libnyoravim
)
