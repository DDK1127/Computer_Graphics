cmake_minimum_required(VERSION 3.16)
project(CampusAnimation CXX)

# C++17 標準
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- 尋找必要函式庫 ----
find_package(GLFW3 REQUIRED)
find_package(glm REQUIRED)

# macOS / Linux OpenGL 設定
if(APPLE)
    # macOS frameworks
    find_library(COCOA_LIB Cocoa REQUIRED)
    find_library(IOKIT_LIB IOKit REQUIRED)
    find_library(COREVIDEO_LIB CoreVideo REQUIRED)
    find_library(OPENGL_FRAMEWORK OpenGL REQUIRED)
    set(PLATFORM_GL_LIB
        ${OPENGL_FRAMEWORK}
        ${COCOA_LIB}
        ${IOKIT_LIB}
        ${COREVIDEO_LIB}
    )
    add_compile_definitions(GL_SILENCE_DEPRECATION)
else()
    # Linux: 系統 OpenGL
    find_package(OpenGL REQUIRED)
    set(PLATFORM_GL_LIB ${OPENGL_LIBRARIES})
endif()

# ---- Include 路徑 ----
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/third_party
)

# ---- Source ----
file(GLOB SRC_FILES
    src/*.cpp
)

add_executable(CampusAnimation ${SRC_FILES})

# ---- Linking ----
target_link_libraries(CampusAnimation
    PRIVATE
        glfw
        ${PLATFORM_GL_LIB}
)

# ---- Build 輸出目錄 ----
set_target_properties(CampusAnimation PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
)

# ---- 給 Linux 提示 ----
if(UNIX AND NOT APPLE)
    message(STATUS "Building for Linux: linking with OpenGL and GLFW3")
endif()
if(APPLE)
    message(STATUS "Building for macOS: using Cocoa + OpenGL frameworks")
endif()

# Auto-detect Apple Silicon
if(APPLE AND CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
    set(CMAKE_OSX_ARCHITECTURES "arm64")
endif()

