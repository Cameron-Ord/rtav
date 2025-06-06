
#include <GL/glew.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matrix.h"

#include <GL/gl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

static FILE *open_file(const char *shader_path, const char *fn);
static int compile_shader(const char *src, unsigned int *shader, GLenum type);
static void sdl_gl_set_flags(void);
static SDL_Window *make_window(void);
static unsigned int attach_shaders(const unsigned int *fshader,
                                   const unsigned int *vshader);
static int check_link_state(const unsigned int *program);
static void gl_viewport_update(SDL_Window *w);

int main(int argc, char **argv) {

  if (argc < 2) {
    fprintf(stdout, "Usage: ./3dmv <directory>\n");
    return 1;
  }

  printf("%s\n", SHADER_PATH);

  FILE *vert = NULL;
  FILE *frag = NULL;

  if (!(vert = open_file(SHADER_PATH, "/vert.vs"))) {
    return 1;
  }

  if (!(frag = open_file(SHADER_PATH, "/frag.fs"))) {
    return 1;
  }

  char vertex_src[2048];
  char frag_src[2048];

  memset(vertex_src, 0, 2048);
  memset(frag_src, 0, 2048);

  int i = 0, j = 0;
  while (fread(&vertex_src[i], 1, 1, vert) > 0 && i < 2048) {
    i++;
  }
  vertex_src[i] = '\0';
  fprintf(stdout, "\nLoaded VERT:\n%s\n", vertex_src);
  fclose(vert);

  while (fread(&frag_src[j], 1, 1, frag) > 0 && j < 2048) {
    j++;
  }
  frag_src[j] = '\0';
  fprintf(stdout, "\nLoaded FRAG:\n%s\n", frag_src);
  fclose(frag);

  if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_VIDEO) < 0) {
    fprintf(stderr, "Could not initialize SDL2: %s\n", SDL_GetError());
    return 1;
  }
  sdl_gl_set_flags();

  SDL_Window *win = NULL;
  if (!(win = make_window())) {
    fprintf(stderr, "Failed to create window : %s\n", SDL_GetError());
    return 1;
  }

  if (!SDL_GL_CreateContext(win)) {
    fprintf(stderr, "Failed to create OpenGL context : %s\n", SDL_GetError());
    return 1;
  }

  if (glewInit() != GLEW_OK) {
    fprintf(stderr, "Failed to initialize libGLEW\n");
    return 1;
  }

  glEnable(GL_BLEND | GL_DEPTH_TEST);

  unsigned int vshader;
  unsigned int fshader;

  compile_shader(vertex_src, &vshader, GL_VERTEX_SHADER);
  compile_shader(frag_src, &fshader, GL_FRAGMENT_SHADER);

  unsigned int sprogram = attach_shaders(&fshader, &vshader);

  glDeleteShader(fshader);
  glDeleteShader(vshader);

  if (!check_link_state(&sprogram)) {
    return 1;
  }

  gl_viewport_update(win);
  SDL_ShowWindow(win);

  int run = 1;
  while (run) {
    const uint32_t start = SDL_GetTicks64();
    glClearColor(0.0, 0.0, 0.0, 1.0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
      default:
        break;

      case SDL_WINDOWEVENT: {
        switch (e.window.event) {
        case SDL_WINDOWEVENT_RESIZED: {
          gl_viewport_update(win);
        } break;
        case SDL_WINDOWEVENT_SIZE_CHANGED: {
          gl_viewport_update(win);
        } break;
        }
      } break;
      case SDL_QUIT: {
        run = 0;
      } break;
      }
    }

    SDL_GL_SwapWindow(win);
    const uint32_t duration = SDL_GetTicks64() - start;
    const uint32_t delta = 1000 / 60;

    if (duration < delta) {
      SDL_Delay(delta - duration);
    }
  }

  return 0;
}

static int check_link_state(const unsigned int *program) {
  int status;
  char log[512];
  glGetProgramiv(*program, GL_LINK_STATUS, &status);
  if (!status) {
    glGetProgramInfoLog(*program, 512, NULL, log);
    fprintf(stderr, "Shader linking failed : %s\n", log);
    return 0;
  }
  return 1;
}

static unsigned int attach_shaders(const unsigned int *fshader,
                                   const unsigned int *vshader) {
  unsigned int program = glCreateProgram();
  glAttachShader(program, *fshader);
  glAttachShader(program, *vshader);
  glLinkProgram(program);
  return program;
}

static SDL_Window *make_window(void) {
  const int flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL;
  SDL_Window *win = SDL_CreateWindow("3DMV", SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED, 1024, 720, flags);
  return win;
}

static void sdl_gl_set_flags(void) {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
}

static int compile_shader(const char *src, unsigned int *shader, GLenum type) {
  const GLchar *glsrc = src;

  *shader = glCreateShader(type);
  glShaderSource(*shader, 1, &glsrc, NULL);
  glCompileShader(*shader);

  int result;
  char log[512];

  glGetShaderiv(*shader, GL_COMPILE_STATUS, &result);
  if (!result) {
    glGetShaderInfoLog(*shader, 512, NULL, log);
    fprintf(stderr, "Shader compilation failed: %s\n", log);
    return 0;
  }

  return 1;
}

static FILE *open_file(const char *path, const char *fn) {
  const size_t spathlen = strlen(path);
  const size_t fnlen = strlen(fn);

  char *fullpath = malloc(spathlen + fnlen + 1);
  if (!fullpath) {
    fprintf(stderr, "Could not allocate memory: %s\n", strerror(errno));
    return NULL;
  }
  memset(fullpath, 0, spathlen + fnlen);

  strncpy(fullpath, path, spathlen);
  strncat(fullpath, fn, fnlen);

  fullpath[spathlen + fnlen] = '\0';

  printf("Opening: %s of path %s\n", fn, path);
  FILE *file = fopen(fullpath, "r");
  if (!file) {
    fprintf(stderr, "Could not open file: %s\n", strerror(errno));
    return NULL;
  }

  free(fullpath);
  return file;
}

static void gl_viewport_update(SDL_Window *w) {
  int width, height;
  SDL_GetWindowSize(w, &width, &height);
  glViewport(0, 0, width, height);
}
