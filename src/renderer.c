#include <GL/glew.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matrix.h"
#include "renderer.h"
#include "rndrdef.h"

#include <GL/gl.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>

const size_t SHADER_SRC_MAX = 2048;
static int shader_src_fill(FILE *file, char *srcbuf);

typedef struct {
  Matrix proj;
  Matrix view;
  Matrix model;
} MatObj;

static MatObj load_mat_obj() {
  MatObj mo = {ortho_mat(0, RENDER_WIDTH, 0, RENDER_HEIGHT, -100, 100),
               identity(), identity()};
  return mo;
}

static void mat_obj_scale(MatObj *const mo, const float model_width,
                          const float model_height) {
  mo->model = multiply_mat(
      scale_mat(model_width, model_height, 15 /*LONG BIG LONG DEEP*/),
      mo->model);
}

static void mat_obj_transform_at(MatObj *const mo, const float xpos,
                                 const float ypos) {
  mo->model = multiply_mat(translate_mat(xpos, ypos, 0.0), mo->model);
}

static void mat_obj_rotate(MatObj *mo) {
  static float angle;
  mo->model = multiply_mat(multiply_mat(rotate_matx(angle), rotate_maty(angle)),
                           mo->model);
  angle = (angle + 0.05f > 360.0f) ? angle - 360.0f : angle + 0.05;
}

static void gl_vertex_bind(const unsigned int VAO) { glBindVertexArray(VAO); }

static void gl_vertex_unbind(void) { glBindVertexArray(0); }

static void gl_prog_use(const unsigned int sid) { glUseProgram(sid); }

static void gl_draw_arrays(GLenum MODE, const int first, const int count) {
  glDrawArrays(MODE, first, count);
}

static void gl_set_uniforms(const unsigned int sid, MatObj *mo) {
  gl_prog_use(sid);

  unsigned int lcloc = glGetUniformLocation(sid, "object_colour");
  glUniform3f(lcloc, 1.0, 1.0, 1.0);

  unsigned int mloc = glGetUniformLocation(sid, "model");
  unsigned int vloc = glGetUniformLocation(sid, "view");
  unsigned int ploc = glGetUniformLocation(sid, "projection");
  // My matrix isnt laid out in memory like opengl expects it so transpose needs
  // to be true
  glUniformMatrix4fv(mloc, 1, GL_TRUE, &mo->model.m0);
  glUniformMatrix4fv(vloc, 1, GL_TRUE, &mo->view.m0);
  glUniformMatrix4fv(ploc, 1, GL_TRUE, &mo->proj.m0);
}
static void light_at(unsigned int sid, const float xpos, const float ypos,
                     const float zpos) {
  unsigned int dloc = glGetUniformLocation(sid, "light_dir");
  unsigned int ocloc = glGetUniformLocation(sid, "light_colour");

  glUniform3f(dloc, xpos, ypos, zpos);
  glUniform3f(ocloc, 0.0f, 0.0f, 1.0f);
}

void gl_draw_buffer(Renderer_Data *rd, const float *sums, const int ww,
                    const int wh) {
  const float model_width = (float)RENDER_WIDTH / DIVISOR;
  const float model_height = (float)RENDER_HEIGHT / DIVISOR;
  light_at(rd->shader_program_id, rd->lightx, rd->lighty, rd->lightz);

  for (int i = 0; i < DIVISOR; i++) {
    const float xpos = i * model_width + model_width / 2;
    const float ypos = sums[i] * (RENDER_HEIGHT * 0.80);
    MatObj m = load_mat_obj();

    mat_obj_scale(&m, model_width * 0.9, model_height);
    mat_obj_rotate(&m);
    mat_obj_transform_at(&m, xpos, ypos);

    gl_set_uniforms(rd->shader_program_id, &m);
    gl_vertex_bind(rd->VAO);
    gl_draw_arrays(GL_TRIANGLES, 0, 36);
    gl_vertex_unbind();
  }
}

static int shader_src_fill(FILE *file, char *srcbuf) {
  int i = 0;
  if (file && srcbuf) {
    memset(srcbuf, 0, SHADER_SRC_MAX * sizeof(char));
    while (fread(&srcbuf[i], 1, 1, file) > 0 && i < SHADER_SRC_MAX) {
      i++;
    }
    srcbuf[i] = '\0';
    fprintf(stdout, "\nLOADED SHADER:\n%s\n", srcbuf);
    fclose(file);
  }
  return i;
}

static float clampf(const float min, const float max, const float sample) {
  if (sample < min) {
    return min;
  }

  if (sample > max) {
    return max;
  }

  return sample;
}

// Todo make each sample a cube, and use FFT

void sdl_gl_set_flags(void) {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
}

void gl_viewport_update(SDL_Window *w, int *ww, int *wh) {
  SDL_GetWindowSize(w, ww, wh);
  // Todo: scale this by multiples/divisions of the base render size dynamically
  int basew = RENDER_WIDTH;
  int baseh = RENDER_HEIGHT;

  float scale = 1.0f;
  while (RENDER_WIDTH * (scale * 1.25) < *ww &&
         RENDER_HEIGHT * (scale * 1.25) < *wh) {
    scale *= 1.25;
  }

  basew = RENDER_WIDTH * scale;
  baseh = RENDER_HEIGHT * scale;

  const int x = (*ww - basew) / 2;
  const int y = (*wh - baseh) / 2;
  glViewport(x, y, basew, baseh);
}

int check_link_state(const unsigned int *program_id) {
  int status;
  char log[512];
  glGetProgramiv(*program_id, GL_LINK_STATUS, &status);
  if (!status) {
    glGetProgramInfoLog(*program_id, 512, NULL, log);
    fprintf(stderr, "Shader linking failed : %s\n", log);
    return 0;
  }
  return 1;
}

FILE *open_shader_src(const char *path, const char *fn) {
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

Renderer_Data load_shaders(void) {
  // Assume it's failed until its proven otherwise
  Renderer_Data rd = {0, 0, 0, 0, 1, 0.0, 0.0, -1.0};
  FILE *fvert = NULL;
  FILE *ffrag = NULL;

  if (!(ffrag = open_shader_src(SHADER_PATH, "/frag.fs"))) {
    return rd;
  }

  if (!(fvert = open_shader_src(SHADER_PATH, "/vert.vs"))) {
    return rd;
  }

  char vertex_src[SHADER_SRC_MAX];
  char frag_src[SHADER_SRC_MAX];

  const int vread = shader_src_fill(fvert, vertex_src);
  const int fread = shader_src_fill(ffrag, frag_src);
  if (!vread || !fread) {
    fprintf(stderr, "Empty shader src\n");
    return rd;
  }

  unsigned int vs, fs;
  if (!compile_shader(vertex_src, &vs, GL_VERTEX_SHADER)) {
    return rd;
  }

  if (!compile_shader(frag_src, &fs, GL_FRAGMENT_SHADER)) {
    return rd;
  }

  rd.shader_program_id = attach_shaders(&fs, &vs);
  glDeleteShader(vs);
  glDeleteShader(fs);

  if (!check_link_state(&rd.shader_program_id)) {
    return rd;
  }

  rd.broken = 0;
  return rd;
}

unsigned int attach_shaders(unsigned int *fs, unsigned int *vs) {
  unsigned int program = glCreateProgram();
  glAttachShader(program, *fs);
  glAttachShader(program, *vs);
  glLinkProgram(program);
  return program;
}

int compile_shader(const char *src, unsigned int *shader, unsigned int type) {
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

void gl_data_construct(Renderer_Data *rd) {
  // 1.618 golden ratio
  // 12 vertices 20 faces icosahedron
  // const float g = (1.0 + sqrt(5.0)) / 2.0;
  // const float scaled = 1.0 / sqrt(1.0 + g * g);
  // const float x = scaled;
  // const float y = g * scaled;
  float vertices[] = {
      -0.5f, -0.5f, -0.5f, 0.0f,  0.0f,  -1.0f, 0.5f,  -0.5f, -0.5f,
      0.0f,  0.0f,  -1.0f, 0.5f,  0.5f,  -0.5f, 0.0f,  0.0f,  -1.0f,
      0.5f,  0.5f,  -0.5f, 0.0f,  0.0f,  -1.0f, -0.5f, 0.5f,  -0.5f,
      0.0f,  0.0f,  -1.0f, -0.5f, -0.5f, -0.5f, 0.0f,  0.0f,  -1.0f,

      -0.5f, -0.5f, 0.5f,  0.0f,  0.0f,  1.0f,  0.5f,  -0.5f, 0.5f,
      0.0f,  0.0f,  1.0f,  0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,
      0.5f,  0.5f,  0.5f,  0.0f,  0.0f,  1.0f,  -0.5f, 0.5f,  0.5f,
      0.0f,  0.0f,  1.0f,  -0.5f, -0.5f, 0.5f,  0.0f,  0.0f,  1.0f,

      -0.5f, 0.5f,  0.5f,  -1.0f, 0.0f,  0.0f,  -0.5f, 0.5f,  -0.5f,
      -1.0f, 0.0f,  0.0f,  -0.5f, -0.5f, -0.5f, -1.0f, 0.0f,  0.0f,
      -0.5f, -0.5f, -0.5f, -1.0f, 0.0f,  0.0f,  -0.5f, -0.5f, 0.5f,
      -1.0f, 0.0f,  0.0f,  -0.5f, 0.5f,  0.5f,  -1.0f, 0.0f,  0.0f,

      0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,  0.5f,  0.5f,  -0.5f,
      1.0f,  0.0f,  0.0f,  0.5f,  -0.5f, -0.5f, 1.0f,  0.0f,  0.0f,
      0.5f,  -0.5f, -0.5f, 1.0f,  0.0f,  0.0f,  0.5f,  -0.5f, 0.5f,
      1.0f,  0.0f,  0.0f,  0.5f,  0.5f,  0.5f,  1.0f,  0.0f,  0.0f,

      -0.5f, -0.5f, -0.5f, 0.0f,  -1.0f, 0.0f,  0.5f,  -0.5f, -0.5f,
      0.0f,  -1.0f, 0.0f,  0.5f,  -0.5f, 0.5f,  0.0f,  -1.0f, 0.0f,
      0.5f,  -0.5f, 0.5f,  0.0f,  -1.0f, 0.0f,  -0.5f, -0.5f, 0.5f,
      0.0f,  -1.0f, 0.0f,  -0.5f, -0.5f, -0.5f, 0.0f,  -1.0f, 0.0f,

      -0.5f, 0.5f,  -0.5f, 0.0f,  1.0f,  0.0f,  0.5f,  0.5f,  -0.5f,
      0.0f,  1.0f,  0.0f,  0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,
      0.5f,  0.5f,  0.5f,  0.0f,  1.0f,  0.0f,  -0.5f, 0.5f,  0.5f,
      0.0f,  1.0f,  0.0f,  -0.5f, 0.5f,  -0.5f, 0.0f,  1.0f,  0.0f};

  glGenVertexArrays(1, &rd->VAO);
  glGenBuffers(1, &rd->VBO);
  glBindBuffer(GL_ARRAY_BUFFER, rd->VBO);
  glBindVertexArray(rd->VAO);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);

  glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                        (void *)(3 * sizeof(float)));
  glEnableVertexAttribArray(1);
}
