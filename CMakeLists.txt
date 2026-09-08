cmake_minimum_required(VERSION 3.16)

project(
    TheRaceCondition
    VERSION 1.0.0
    DESCRIPTION "Standalone POSIX thread race condition examples"
    LANGUAGES CXX
)

if(NOT CMAKE_CONFIGURATION_TYPES AND NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the build type." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

find_package(Threads REQUIRED)

add_subdirectory(example)
