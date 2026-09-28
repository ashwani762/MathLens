# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (c) 2026 Ashwani

cmake_minimum_required(VERSION 3.22)
project(MathEngine VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(MATHENGINE_BUILD_TOOLS "Build the command line tool and tests" ON)

file(GLOB_RECURSE MATHENGINE_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_library(mathengine STATIC ${MATHENGINE_SOURCES})
target_include_directories(mathengine
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
set_target_properties(mathengine PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Built without exceptions: errors travel through std::expected / std::optional.
if(MSVC)
    target_compile_options(mathengine PRIVATE /EHs-c- /W4)
    target_compile_definitions(mathengine PRIVATE _HAS_EXCEPTIONS=0)
else()
    target_compile_options(mathengine PRIVATE -fno-exceptions -Wall -Wextra -Wno-unused-parameter)
endif()

if(MATHENGINE_BUILD_TOOLS)
    add_executable(mathcli cli/main.cpp)
    target_link_libraries(mathcli PRIVATE mathengine)

    add_executable(mathtests tests/tests.cpp)
    target_link_libraries(mathtests PRIVATE mathengine)
    enable_testing()
    add_test(NAME mathtests COMMAND mathtests)
endif()
