
#include <GL/glew.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "audio.h"
#include "entry.h"
#include "matrix.h"

#include <GL/gl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

typedef struct {
  unsigned int VBO, VAO, EBO;
  unsigned int shader;
  Matrix proj;
  Matrix model;
  Matrix view;
} glData;

static FILE *open_file(const char *shader_path, const char *fn);
static int compile_shader(const char *src, unsigned int *shader, GLenum type);
static void sdl_gl_set_flags(void);
static SDL_Window *make_window(void);
static unsigned int attach_shaders(const unsigned int *fshader,
                                   const unsigned int *vshader);
static int check_link_state(const unsigned int *program);
static void gl_viewport_update(SDL_Window *w, int *ww, int *wh);
static AParams *begin_audio_file(const Entry *const e);
static AParams *__begin_bad(AParams *p);
static AParams *__begin_ok(AParams *p);
static void *free_params(AParams *p);

static void gl_data_construct(glData *gd);

int main(int argc, char **argv) {

  if (argc < 2 || argc > 2) {
    fprintf(stdout, "Usage: ./3dmv <directory>\n");
    return 0;
  }

  const char *directory = argv[1];
  Entries ents = read_directory(directory);

  if (ents.size == 0) {
    fprintf(stdout, "Empty/Non-existant diretory\n");
    return 0;
  }

  // Todo : cleanup if this happens.
  if (ents.malformed) {
    fprintf(stderr, "Error occured while reading dir entries\n");
    return 1;
  }

  if (parse_headers(&ents) == 0) {
    fprintf(stdout, "Directory contains no audio files\n");
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

  // Track and set dimension values
  int ww, wh;
  gl_viewport_update(win, &ww, &wh);
  glEnable(GL_BLEND | GL_DEPTH_TEST);

  unsigned int vshader;
  unsigned int fshader;

  compile_shader(vertex_src, &vshader, GL_VERTEX_SHADER);
  compile_shader(frag_src, &fshader, GL_FRAGMENT_SHADER);

  glData gd = {0};
  gd.shader = attach_shaders(&fshader, &vshader);

  glDeleteShader(fshader);
  glDeleteShader(vshader);

  if (!check_link_state(&gd.shader)) {
    return 1;
  }
  gl_data_construct(&gd);

  Entry *const estart = ents.list;
  Entry *const eend = ents.list + ents.size;
  Entry *current = ents.list;

  AParams *p = begin_audio_file(current);

  SDL_ShowWindow(win);
  int song_queued = 0;
  int run = 1;

  while (run) {
    const uint32_t start = SDL_GetTicks64();

    if (p && !callback_check_pos(p->len, p->position)) {
      audio_end();
      p = free_params(p);
      song_queued = 1;
    }

    if (!p && song_queued) {
      current = (current + 1 != eend) ? current + 1 : estart;
      if ((p = begin_audio_file(current))) {
        song_queued = 0;
      }
    }

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
          gl_viewport_update(win, &ww, &wh);
        } break;
        case SDL_WINDOWEVENT_SIZE_CHANGED: {
          gl_viewport_update(win, &ww, &wh);
        } break;
        }
      } break;
      case SDL_QUIT: {
        run = 0;
      } break;
      }
    }

    Matrix proj = pers_mat(45.0f, (float)ww / wh, 0.1f, 100.0f);
    Matrix model = identity();
    Matrix view = identity();
    model = multiply_mat(model, translate_mat(0.0f, 0.0f, -3.0f));

    glUseProgram(gd.shader);

    unsigned int cloc = glGetUniformLocation(gd.shader, "colour");
    unsigned int mloc = glGetUniformLocation(gd.shader, "model");
    unsigned int vloc = glGetUniformLocation(gd.shader, "view");
    unsigned int ploc = glGetUniformLocation(gd.shader, "projection");

    glUniformMatrix4fv(mloc, 1, GL_TRUE, &model.m0);
    glUniformMatrix4fv(vloc, 1, GL_TRUE, &view.m0);
    glUniformMatrix4fv(ploc, 1, GL_TRUE, &proj.m0);
    glUniform4f(cloc, 1.0f, 1.0f, 1.0f, 1.0f);

    glBindVertexArray(gd.VAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

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

static void gl_viewport_update(SDL_Window *w, int *ww, int *wh) {
  SDL_GetWindowSize(w, ww, wh);
  fprintf(stdout, "Output updated w: %d h: %d\n", *ww, *wh);
  glViewport(0, 0, *ww, *wh);
}

static AParams *__begin_bad(AParams *p) {
  const int cond = p != NULL;
  switch (cond) {
  default: {
    return NULL;
  } break;

  case 1: {
    return free_params(p);
  } break;
  }
}

static AParams *__begin_ok(AParams *p) {
  audio_start();
  return p;
}

static AParams *begin_audio_file(const Entry *const e) {
  if (e && e->is_audio_file) {
    AParams *p = read_file(e->fullpath);
    if ((p && p->valid) && dev_from_data(p)) {
      return __begin_ok(p);
    } else {
      return __begin_bad(p);
    }
  }
  return NULL;
}

static void *free_params(AParams *p) {
  if (p) {
    if (p->buffer) {
      free(p->buffer);
    }

    free(p);
  }
  return NULL;
}

static void gl_data_construct(glData *gd) {

  // 1.618 golden ratio
  // 12 vertices 20 faces icosahedron
  // const float g = (1.0 + sqrt(5.0)) / 2.0;
  // const float scaled = 1.0 / sqrt(1.0 + g * g);

  // const float x = scaled;
  // const float y = g * scaled;

  const float vertices[] = {
      // top 0
      0.0f,
      1.0f,
      0.0f,
      // bottom 1
      0.0f,
      -1.0f,
      0.0f,
      // front 2
      0.0f,
      0.0f,
      1.0f,
      // back 3
      0.0f,
      0.0f,
      -1.0f,
      // left 4
      -1.0f,
      0.0f,
      0.0f,
      // right 5
      1.0f,
      0.0f,
      0.0f,
  };

  unsigned int indices[] = {// top font right
                            0, 2, 5,
                            // top right back
                            0, 5, 3,
                            // top back left
                            0, 3, 4,
                            // top left front
                            0, 4, 2,

                            // bottom right front
                            1, 5, 2,
                            // bottom back right
                            1, 3, 5,
                            // bottom left back
                            1, 4, 3,
                            // bottom front left
                            1, 2, 4};

  glGenVertexArrays(1, &gd->VAO);

  glGenBuffers(1, &gd->VBO);
  glGenBuffers(1, &gd->EBO);

  glBindVertexArray(gd->VAO);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gd->EBO);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices,
               GL_STATIC_DRAW);
  glBindBuffer(GL_ARRAY_BUFFER, gd->VBO);
  // Data doesnt change
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
}
