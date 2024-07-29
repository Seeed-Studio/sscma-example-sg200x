cmake_minimum_required(VERSION 3.5.0)

get_filename_component(PROJECT_DIR ${CMAKE_CURRENT_LIST_DIR} ABSOLUTE)

message(STATUS "PROJECT_DIR: ${PROJECT_DIR}")

get_filename_component(ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../" ABSOLUTE)

message(STATUS "ROOT_DIR: ${ROOT_DIR}")

include(${ROOT_DIR}/cmake/toolchain-riscv64-linux-musl-x86_64.cmake)

project(mqtt_camera C CXX)

include(${ROOT_DIR}/cmake/project.cmake)




