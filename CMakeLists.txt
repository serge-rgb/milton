cmake_minimum_required(VERSION 3.1)
project(Milton)

if(WIN32)
  set(DIRECTX 0)
endif()


add_executable(shadergen
  src/shadergen.cc
)

FILE (GLOB ShaderSources src/*.glsl third_party/*.glsl)

add_executable(Milton WIN32 #MACOSX_BUNDLE
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

if(WIN32)
  target_sources(Milton PRIVATE
    src/platform_windows.cc
  )
endif()

if(UNIX)
  set(UnixCFlags
    -g
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
