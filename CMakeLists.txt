cmake_minimum_required(VERSION 3.1)
project(Milton)

if(APPLE)
  add_subdirectory(third_party/SDL2-2.0.3)
endif()

add_executable(shadergen
  src/shadergen.cc
)

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
  src/hardware_renderer.cc
  src/utils.cc
  src/hash.cc
  src/vector.cc
  src/sdl_milton.cc
  src/StrokeList.cc
  src/third_party_libs.cc
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
  target_sources(Milton PRIVATE
    src/platform_unix.cc
  )
  target_compile_options(Milton PRIVATE
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
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  target_sources(Milton PRIVATE
    src/platform_linux.cc
  )
endif()

if(APPLE)
  target_sources(Milton PRIVATE
    src/platform_mac.cc
  )
  target_link_libraries(Milton
    SDL2-static
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

add_custom_command(TARGET Milton PRE_BUILD
  COMMAND $<TARGET_FILE:shadergen>
  WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
)
