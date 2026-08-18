cmake_minimum_required(VERSION 3.20)
project(Argos LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "Argos only builds for Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(argos_engine STATIC
    src/engine/placeholder.cpp
)
target_include_directories(argos_engine PUBLIC src)
target_compile_definitions(argos_engine PUBLIC UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_link_libraries(argos_engine PUBLIC d2d1 dwrite gdi32 user32 shcore)

add_executable(argos_engine_demo WIN32
    src/demo/main.cpp
)
target_link_libraries(argos_engine_demo PRIVATE argos_engine)
