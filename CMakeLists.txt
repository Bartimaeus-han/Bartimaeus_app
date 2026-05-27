# CMake : Build script writing tools for automating complex build process

# Specify minimum CMake version and project name
cmake_minimum_required(VERSION 3.15)
project(SecureWebServer CXX)        # CXX means this project using C++ language

# Set use C++20 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set external library auto download and synchronize
# FetchContent : A module among CMake's built-in features that directly fetches libraries
#                from the internet and integrates them into the build.
include(FetchContent)
# declare information about external libraries to import
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG       v0.18.3  # 
)
# download and get to build setting a httplib
# For this step, we can use `httplib` in our project
FetchContent_MakeAvailable(httplib)

# make variable named `SOURCES` and store source file list to compile
set(SOURCES
    src/main.cpp
)

# Create execute file
# `SecureWebServer` is final execute file (in Winodws, .exe)
# In CMake, `SecureWebServer` called **Target**
add_executable(SecureWebServer ${SOURCES})

# Link cpp-httplib library (auto binding header path and socket library) to **Target**
# PRIVATE : use only in **Target**
target_link_libraries(SecureWebServer PRIVATE httplib::httplib)