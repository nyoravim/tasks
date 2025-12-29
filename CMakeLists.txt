cmake_minimum_required(VERSION 3.20)
project(tasks LANGUAGES C)

file(GLOB_RECURSE SRC "src/*.c")
add_executable(tasks ${SRC})

target_link_libraries(
    tasks PRIVATE 
    pthread
    discord
    curl
    cjson
)
