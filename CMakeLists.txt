cmake_minimum_required(VERSION 3.10)
project(3DMV VERSION 1.0)
#define 3DMV_VERSION_MAJOR @3DMV_VERSION_MAJOR@
#define 3DMV_VERSION_MINOR @3DMV_VERSION_MINOR@

set(SRCS src/main.c)
add_executable(3DMV ${SRCS})

set(SHADER_DIR "/usr/local/share/3dmv" CACHE PATH "Path to shader files")
target_compile_definitions(3DMV PRIVATE SHADER_PATH=\"${SHADER_DIR}\")
message("Shader dir is: ${SHADER_DIR}")

find_library(SDL2_LIB NAMES SDL SDL2 sdl2 libsdl2 sdl)
find_library(OPENGL_LIB NAMES OpenGL opengl LIBGL libGL LibGL libgl gl)
find_library(SND_LIB NAMES libsndfile sndfile)
find_library(GLEW_LIB NAMES LibGLEW glew libGLEW LIBGLEW GLEW)

if(NOT SDL2_LIB)
    message(FATAL_ERROR "SDL2 Library not found!")
endif()

if(NOT OPENGL_LIB)
    message(FATAL_ERROR "LibGL not found!")
endif()

if(NOT SND_LIB)
    message(FATAL_ERROR "libsndfile not found!")
endif()

if(NOT GLEW_LIB)
    message(FATAL_ERROR "libGLEW not found!")
endif()

set(LIBS ${SDL2_LIB} ${OPENGL_LIB} ${GLEW_LIB} ${SND_LIB})
target_link_libraries(3DMV PRIVATE ${LIBS})
