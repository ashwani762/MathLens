# SPDX-License-Identifier: MIT
# Copyright (c) 2026 Ashwani

cmake_minimum_required(VERSION 3.22)
project(mathlens_jni LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# The portable engine lives outside the Android project so it can be reused on other platforms.
set(MATHENGINE_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../../../../mathengine ${CMAKE_BINARY_DIR}/mathengine)

add_library(mathlens SHARED jni_bridge.cpp)
target_link_libraries(mathlens PRIVATE mathengine log)
target_compile_options(mathlens PRIVATE -fno-exceptions -O2)
# optimise the engine in every build type (debug APKs included) so solving is always instant
target_compile_options(mathengine PRIVATE -O2)
target_link_options(mathlens PRIVATE -Wl,--gc-sections -Wl,-z,max-page-size=16384)
