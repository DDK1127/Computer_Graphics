cmake_minimum_required(VERSION 3.12)
project(hw2_textured_mesh LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
  pkg_check_modules(GLFW3 QUIET glfw3)
endif()

if(GLFW3_FOUND)
  message(STATUS "Using GLFW from pkg-config")
  include_directories(${GLFW3_INCLUDE_DIRS})
  link_libraries(${GLFW3_STATIC_LIBRARIES} ${GLFW3_LIBRARIES})
else()
  find_package(glfw3 REQUIRED)
endif()

# 指定 glad 的語言明確為 C
add_library(glad STATIC external/glad/src/glad.c)
set_target_properties(glad PROPERTIES LINKER_LANGUAGE C)
target_include_directories(glad PUBLIC external/glad/include)

add_executable(hw2 src/main.cpp)
target_include_directories(hw2 PRIVATE external)

target_link_libraries(hw2 PRIVATE glad glfw)

if(APPLE)
  target_link_libraries(hw2 PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
endif()

add_custom_command(TARGET hw2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:hw2>/assets)
add_custom_command(TARGET hw2 POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_SOURCE_DIR}/src/shaders
          $<TARGET_FILE_DIR:hw2>/shaders)