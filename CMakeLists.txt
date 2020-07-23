cmake_minimum_required(VERSION 3.12)

# Project
project(cpputils VERSION 0.0.1
                 DESCRIPTION "C++ Utils"
                 LANGUAGES CXX
                 HOMEPAGE_URL "https://github.com/NicholasJarr/cpputils")

# Options
option(BUILD_WEB "Build the web lib" ON)

if (BUILD_WEB)
    add_subdirectory(web)
    add_library(${PROJECT_NAME}::web ALIAS web)
endif()
