cmake_minimum_required(VERSION 3.1)
project(Milton)

if(WIN32)
  set(DIRECTX 0)
endif()


add_executable(shadergen
  src/shadergen.cc
)

FILE (GLOB ShaderSources src/*.glsl third_party/*.glsl)

add_executable(Milton WIN32 MACOSX_BUNDLE
  src/milton.cc
  src/memory.cc
  src/gui.cc
  src/persist.cc
  src/color.cc
  src/canvas.cc
  src/profiler.cc
  src/gl_helpers.cc
  src/localization.cc
  src/renderer.cc
  src/utils.cc
  src/vector.cc
  src/sdl_milton.cc
  src/StrokeList.cc
  src/third_party_libs.cc

  src/shaders.gen.h
)

target_include_directories(Milton PRIVATE
  src
  third_party
  third_party/imgui
)

# Handle various switches, build types etc.

## Helps to know that we're compiling using cmake in milton_configuration.h
add_definitions(-DIS_USING_CMAKE=1)

if(TRY_GL2)
  message(STATUS "Compiling for OpenGL2.1")
  add_definitions(-DCMAKE_TRY_GL2=1)
elseif(TRY_GL3)
  message(STATUS "Compiling for OpenGL3.2")
  add_definitions(-DCMAKE_TRY_GL3=1)
endif()

## Default build type to Release
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release")
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
  add_definitions(-DMILTON_DEBUG=1)
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_definitions(-DMILTON_DEBUG=0)
else()
  message(FATAL_ERROR "Unknown build type: ${CMAKE_BUILD_TYPE}. Supported build types: Release, Debug.")
endif()

set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

message(STATUS "Building ${CMAKE_BUILD_TYPE}")

# Platform specific stuff
if(WIN32)
  target_sources(Milton PRIVATE
    src/platform_windows.cc
  )
endif()

if(UNIX)
  set(UnixCFlags
    -std=c++11
    -Wno-missing-braces
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-result
    -Wno-write-strings
    -Wno-c++11-compat-deprecated-writable-strings
    -Wno-null-dereference
    -Wno-format
    -fno-strict-aliasing
    -fno-omit-frame-pointer
    -Wno-extern-c-compat
    -Werror
    )
  target_sources(Milton PRIVATE
    src/platform_unix.cc
  )
  target_compile_options(shadergen PRIVATE
    ${UnixCFlags})
  target_compile_options(Milton PRIVATE
    ${UnixCFlags})
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

  target_sources(Milton PRIVATE
    src/platform_linux.cc
  )

  find_package(OpenGL REQUIRED)
  find_package(SDL2 REQUIRED)
  find_package(GTK2 2.6 REQUIRED gtk)
  find_package(X11 REQUIRED)
  find_library(XINPUT_LIBRARY libXi.so)

  if(XINPUT_LIBRARY STREQUAL "XINPUT_LIBRARY-NOTFOUND")
      message(FATAL_ERROR "Could not find libXi.so")
  endif()

  if(NOT OPENGL_FOUND)
      message(FATAL_ERROR "Could not find OpenGl libraries")
  endif()

  if(NOT GTK2_FOUND)
    message(FATAL_ERROR "Could not find GTK2.8 libraries")
  endif()

  if(NOT X11_FOUND)
    message(FATAL_ERROR "Could not find X11 libraries")
  endif()

  if(NOT SDL2_FOUND)
    message(FATAL_ERROR "Could not find SDL2 libraries")
  endif()

  # on ubuntu 16.04 LTS CMake complains about the -lSDL2 link switch
  # having trailing/leading spaces.
  # string(STRIP a b) will remove those spaces.
  string(STRIP ${SDL2_LIBRARIES} SDL2_LIBRARIES)

  target_include_directories(Milton PRIVATE
    ${GTK2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
    ${SDL2_INCLUDE_DIRS}
    ${OPENGL_INCLUDE_DIR}
  )

  target_link_libraries(Milton
    ${GTK2_LIBRARIES}
    ${X11_LIBRARIES}
    ${SDL2_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${XINPUT_LIBRARY})

else()
  add_subdirectory(third_party/SDL2-2.0.3)
  target_link_libraries(Milton SDL2-static)
endif()

if(APPLE)
  target_sources(Milton PRIVATE
    src/platform_mac.mm
  )
  target_link_libraries(Milton
    "-framework OpenGL"
  )
endif()


if(WIN32 OR APPLE)
  target_include_directories(Milton PRIVATE
    third_party/SDL2-2.0.3/include
  )
endif()

add_custom_command(TARGET Milton POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy
    ${CMAKE_CURRENT_LIST_DIR}/milton_icon.ico
    ${CMAKE_CURRENT_LIST_DIR}/third_party/Carlito.ttf
    ${CMAKE_CURRENT_LIST_DIR}/third_party/Carlito.LICENSE
    $<TARGET_FILE_DIR:Milton>
)

add_dependencies(Milton shadergen)


add_custom_command(
  COMMAND $<TARGET_FILE:shadergen>
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
  DEPENDS ${ShaderSources}
  OUTPUT src/shaders.gen.h
)
