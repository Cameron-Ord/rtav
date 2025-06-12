cmake_minimum_required(VERSION 3.10)
project(rtav VERSION 1.0)

set(SRCS src/main.c src/sys.c src/audio.c src/matrix.c src/renderer.c src/fft.c)
add_executable(rtav ${SRCS})

set(SHADER_DIR "/usr/local/share/rtav" CACHE PATH "Path to shader files")
target_compile_definitions(rtav PRIVATE SHADER_PATH=\"${SHADER_DIR}\")
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

set(LIBS ${SDL2_LIB} ${OPENGL_LIB} ${GLEW_LIB} ${SND_LIB} m)
target_link_libraries(rtav PRIVATE ${LIBS})
