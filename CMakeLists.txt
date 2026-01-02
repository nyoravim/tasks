cmake_minimum_required(VERSION 3.20)
project(tasks LANGUAGES C)

add_subdirectory("vendor")

file(GLOB_RECURSE SRC "src/*.c")
add_executable(tasks ${SRC})

find_package(json-c REQUIRED)
find_package(libnyoravim REQUIRED)
find_package(CURL REQUIRED)

target_link_libraries(
    tasks PRIVATE 

    # submodules
    tasks_log

    # system libraries
    pthread
    libnyoravim
    discord
    CURL::libcurl
    json-c::json-c
    hiredis
)
