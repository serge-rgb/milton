cmake_minimum_required(VERSION 3.1)
project(Milton)

if(WIN32)
  set(DIRECTX 0)
endif()


add_executable(shadergen
  src/shadergen.cc
)

set(SDL2DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/SDL2-2.0.8)

FILE (GLOB ShaderSources src/*.glsl third_party/*.glsl)

add_executable(Milton WIN32 MACOSX_BUNDLE
  src/unity.cc
  src/shaders.gen.h
)

target_include_directories(Milton PRIVATE
  src
  third_party
  third_party/imgui
)

# Handle various switches, build types etc.

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
    #-Werror
    )
  if (APPLE)
      set(UnixCFlags "${UnixCFlags}" -ObjC++)
  endif()

  target_compile_options(shadergen PRIVATE
    ${UnixCFlags})
  target_compile_options(Milton PRIVATE
    ${UnixCFlags})
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")

  find_package(OpenGL REQUIRED)
  find_package(GTK2 2.6 REQUIRED gtk)
  find_package(X11 REQUIRED)
  find_library(XINPUT_LIBRARY libXi.so)
  find_package(Threads REQUIRED)

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

  target_include_directories(Milton PRIVATE
    ${GTK2_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
    ${SDL2DIR}/build/linux64/include/SDL2
    ${OPENGL_INCLUDE_DIR}
  )

  target_link_libraries(Milton
    ${GTK2_LIBRARIES}
    ${X11_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${XINPUT_LIBRARY}
    ${SDL2DIR}/build/linux64/lib/libSDL2maind.a
    ${SDL2DIR}/build/linux64/lib/libSDL2d.a
    ${CMAKE_THREAD_LIBS_INIT}
    ${CMAKE_DL_LIBS}
    )

else()
  add_subdirectory(${SDL2DIR})
  target_link_libraries(Milton SDL2-static)
endif()

if(APPLE)
  target_link_libraries(Milton
    "-framework OpenGL"
  )
endif()


if(WIN32 OR APPLE)
  target_include_directories(Milton PRIVATE
    ${SDL2DIR}/include
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
