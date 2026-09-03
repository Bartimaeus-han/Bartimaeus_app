add_executable(ReverseProxy
    main.cpp
)

target_compile_features(ReverseProxy PRIVATE cxx_std_20)

if(WIN32)
    target_link_libraries(ReverseProxy PRIVATE ws2_32)
endif()