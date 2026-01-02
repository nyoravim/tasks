cmake_minimum_required(VERSION 3.20)

set(LOG_DIR "${CMAKE_CURRENT_SOURCE_DIR}/log.c/src")
add_library(tasks_log STATIC "${LOG_DIR}/log.c")
target_include_directories(tasks_log PUBLIC ${LOG_DIR})
