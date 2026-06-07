# CMake : Build script writing tools for automating complex build process

# Specify minimum CMake version and project name
cmake_minimum_required(VERSION 3.15)
# CXX means this project using C++ language
# C means using C language too.
project(SecureWebServer C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set use C++17 standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Set external library auto download and synchronize
# FetchContent : A module among CMake's built-in features that directly fetches libraries
#                from the internet and integrates them into the build.
include(FetchContent)
# declare information about external libraries to import
FetchContent_Declare(
    httplib
    # This address is official repository
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG       v0.18.3  # It points to a specific version
)
# download and get to build setting a httplib
# For this step, we can use `httplib` in our project
FetchContent_MakeAvailable(httplib)

# This method have 'supply chain attack vulnerabilities' 
# FetchContent_Declare(
#     sqlite3
#     GIT_REPOSITORY https://github.com/azadkuh/sqlite-amalgamation.git
#     GIT_TAG master  # master branch is a place that is constantly updated.
# )
# FetchContent_MakeAvailable(sqlite3)

# make variable named `SOURCES` and store source file list to compile
set(SOURCES
    src/main.cpp
    src/sqlite3.c   # Include directly in the build target
)

# Create execute file
# `SecureWebServer` is final execute file (in Winodws, .exe)
# In CMake, `SecureWebServer` called **Target**
add_executable(SecureWebServer ${SOURCES}
    src/services/session_manager.hpp
    src/services/login_limiter.hpp)

# Link cpp-httplib library (auto binding header path and socket library) to **Target**
# PRIVATE : use only in **Target**
target_link_libraries(SecureWebServer PRIVATE httplib::httplib)

target_include_directories(SecureWebServer PRIVATE src)
