# CMake : Build script writing tools for automating complex build process

# Specify minimum CMake version and project name
cmake_minimum_required(VERSION 3.15)
# CXX means this project using C++ language
# C means using C language too.
project(SecureWebServer C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set use C++17 standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------------------------------------------------------------
# Local 3rdparty Library Configurations (Secure & Offline Build Support)
# ----------------------------------------------------------------------
# 1. cpp-httplib configuration
add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/cpp-httplib)
add_library(httplib::httplib ALIAS httplib)

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
    3rdparty/sqlite3/sqlite3.c   # Include directly in the build target
)

# Search for OpenSSL package
set(OPENSSL_ROOT_DIR "C:/Users/Bartimaeus/anaconda3/Library")
find_package(OpenSSL REQUIRED)

# Create execute file
# `SecureWebServer` is final execute file (in Winodws, .exe)
# In CMake, `SecureWebServer` called **Target**
add_executable(SecureWebServer ${SOURCES}
    src/middleware.hpp
    src/services/session_manager.hpp
    src/services/login_limiter.hpp
    src/services/board_service.hpp
    src/controllers/board_controller.hpp
    src/controllers/auth_controller.hpp)

# Link OpenSSL and cpp-httplib libraries
target_link_libraries(SecureWebServer PRIVATE httplib::httplib OpenSSL::SSL OpenSSL::Crypto)

# Enable OpenSSL support macro definition
target_compile_definitions(SecureWebServer PRIVATE CPPHTTPLIB_OPENSSL_SUPPORT)

target_include_directories(SecureWebServer PRIVATE 
    src
    3rdparty/sqlite3
    3rdparty/picosha2
)
