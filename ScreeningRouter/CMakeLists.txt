# Define Screening router executable
add_executable(ScreeningRouter
    main.cpp
)

# Enable C++20 standard
target_compile_features(ScreeningRouter PRIVATE cxx_std_20)

# Link Winsock library for Windows build
if(WIN32) 
    target_link_libraries(ScreeningRouter PRIVATE ws2_32)
endif()