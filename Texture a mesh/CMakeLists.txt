cmake_minimum_required(VERSION 3.12)
project(hw2_textured_mesh LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#===========================================================
# 相依套件：GLFW + GLAD
#===========================================================

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(GLFW3 QUIET glfw3)
endif()

if(GLFW3_FOUND)
  message(STATUS "Using GLFW from pkg-config")
  include_directories(${GLFW3_INCLUDE_DIRS})
  link_libraries(${GLFW3_LIBRARIES})
else()
  # Windows or systems without pkg-config
  find_path(GLFW_INCLUDE_DIR GLFW/glfw3.h)
  find_library(GLFW_LIBRARY NAMES glfw3 glfw)
  if(NOT GLFW_INCLUDE_DIR OR NOT GLFW_LIBRARY)
    message(FATAL_ERROR "GLFW not found. Please install glfw3 (e.g., apt install libglfw3-dev / brew install glfw / vcpkg install glfw3).")
  endif()
  include_directories(${GLFW_INCLUDE_DIR})
  set(GLFW3_LIBRARIES ${GLFW_LIBRARY})
endif()

#===========================================================
# GLAD
#===========================================================

add_library(glad STATIC external/glad/src/glad.c)
set_target_properties(glad PROPERTIES LINKER_LANGUAGE C)
target_include_directories(glad PUBLIC external/glad/include)

#===========================================================
# 主程式模板：重用 main.cpp，透過編譯常數選不同模型
#===========================================================

function(make_viewer TARGET_NAME MODEL_FILE)
  add_executable(${TARGET_NAME} src/main.cpp)
  target_include_directories(${TARGET_NAME} PRIVATE external)
  target_link_libraries(${TARGET_NAME} PRIVATE glad ${GLFW3_LIBRARIES})
  target_compile_definitions(${TARGET_NAME} PRIVATE MODEL_FILE="${MODEL_FILE}")

  if(APPLE)
    target_link_libraries(${TARGET_NAME} PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
  endif()

  # --- 自動複製 assets 與 shaders ---
  add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/assets
            $<TARGET_FILE_DIR:${TARGET_NAME}>/assets)

  add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/src/shaders
            $<TARGET_FILE_DIR:${TARGET_NAME}>/shaders)
endfunction()

#===========================================================
# 生成兩個 viewer：Tiger 與 Buddha
#===========================================================
make_viewer(tiger_viewer "tiger.obj")
make_viewer(buddha_viewer "buddha.obj")
