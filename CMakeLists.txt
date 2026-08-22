cmake_minimum_required(VERSION 3.20)
project(Argos LANGUAGES CXX)

if(NOT WIN32)
  message(FATAL_ERROR "Argos only builds for Windows.")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(argos_engine STATIC
    src/engine/D2DContext.cpp
    src/engine/LayeredWindow.cpp
    src/engine/MonitorUtil.cpp
)
target_include_directories(argos_engine PUBLIC src)
target_compile_definitions(argos_engine PUBLIC UNICODE _UNICODE _WIN32_WINNT=0x0A00 WINVER=0x0A00)
target_link_libraries(argos_engine PUBLIC d2d1 dwrite gdi32 user32 shcore)

add_executable(argos_engine_demo WIN32
    src/demo/main.cpp
)
target_link_libraries(argos_engine_demo PRIVATE argos_engine)

if(MSVC)
  # /MANIFEST:EMBED is incompatible with /INCREMENTAL (on by default for Debug
  # builds); without /INCREMENTAL:NO, CMake's link wrapper still generates a
  # second manifest via mt.exe and the two collide (LNK1123 duplicate
  # resource). Forcing /INCREMENTAL:NO here makes the embed path the only one.
  set_target_properties(argos_engine_demo PROPERTIES
      LINK_FLAGS "/MANIFEST:EMBED /MANIFESTINPUT:\"${CMAKE_SOURCE_DIR}/app.manifest\" /INCREMENTAL:NO")
endif()

# Pure logic, no Windows/D2D dependency -- compiled as its own tiny target
# (not linked against argos_engine or argos_skin) so it builds fast and
# stays a true unit test of the parser alone.
add_executable(argos_ini_parser_test
    src/skin/IniParser.cpp
    src/skin/IniParserTest.cpp
)
target_include_directories(argos_ini_parser_test PRIVATE src)

add_library(argos_skin STATIC
    src/skin/IniParser.cpp
    src/skin/Skin.cpp
    src/measures/Measure.cpp
    src/meters/Meter.cpp
)
target_include_directories(argos_skin PUBLIC src)
target_link_libraries(argos_skin PUBLIC argos_engine)

add_executable(argos_skin_demo WIN32
    src/demo/skin_demo.cpp
)
target_link_libraries(argos_skin_demo PRIVATE argos_skin)
if(MSVC)
  set_target_properties(argos_skin_demo PROPERTIES
      LINK_FLAGS "/MANIFEST:EMBED /MANIFESTINPUT:\"${CMAKE_SOURCE_DIR}/app.manifest\" /INCREMENTAL:NO")
endif()
