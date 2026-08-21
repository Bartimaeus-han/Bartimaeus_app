add_executable(ScreeningRouter
    main.cpp
)

target_compile_features(ScreeningRouter PRIVATE cxx_std_20)

if(WIN32)
    target_link_libraries(ScreeningRouter PRIVATE ws2_32)
endif()