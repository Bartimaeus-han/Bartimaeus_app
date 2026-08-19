# Define build targets for attack simulation tools

# Ochlos 하위의 모든 cpp 파일을 자동으로 탐색해버린다

file(GLOB_RECURSE OCHLOS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# 각 cpp 파일별로 독립 실행 파일 타깃 자동 생성
foreach(SOURCE_FILE ${OCHLOS_SOURCES})
    get_filename_component(TARGET_NAME ${SOURCE_FILE} NAME_WE)

    add_executable(${TARGET_NAME} ${SOURCE_FILE})

    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)

    if(WIN32)
        target_link_libraries(${TARGET_NAME} PRIVATE ws2_32)
    endif()
endforeach()