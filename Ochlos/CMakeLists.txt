# Define build targets for attack simulation tools
cmake_minimum_required(VERSION 3.15)
project(Ochlos CXX)

# editor 자동완성을 위하여 compile command 내보내기 활성화
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# MSVC 한글 주석 경고 방지
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Ochlos 하위 모든 .cpp 파일 자동 탐색
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