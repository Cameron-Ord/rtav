#include <GL/glew.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matrix.h"
#include "renderer.h"

#include <GL/gl.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>

const size_t SHADER_SRC_MAX = 2048;
static int shader_src_fill(FILE *file, char *srcbuf);

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
void gl_draw_buffer(Renderer_Data *rd, const float *sums, const int bcount,
                    const int ww, const int wh) {
  static float rangle;
  for (int i = 0; i < bcount; i++) {
    const float angle = (2.0 * PI * i) / bcount;

    float radius = 3.75f;
    const float x = cosf(angle) * radius;
    const float y = sinf(angle) * radius;

    Matrix proj = pers_mat(45.0f, (float)ww / wh, 0.1f, 100.0f);
    Matrix view = identity();
    Matrix model = identity();

    Matrix rotx = rotate_matx(rangle + sums[i]);
    Matrix roty = rotate_maty(rangle + sums[i]);

    model = multiply_mat(scale_mat(clampf(0.5, 1.1f, 1.0f * sums[i])), model);
    model = multiply_mat(multiply_mat(rotx, roty), model);
    model = multiply_mat(translate_mat(x, y, -10.0), model);

    const unsigned int sid = rd->shader_program_id;
    glUseProgram(sid);

    unsigned int cloc = glGetUniformLocation(sid, "colour");
    unsigned int mloc = glGetUniformLocation(sid, "model");
    unsigned int vloc = glGetUniformLocation(sid, "view");
    unsigned int ploc = glGetUniformLocation(sid, "projection");

    glUniformMatrix4fv(mloc, 1, GL_TRUE, &model.m0);
    glUniformMatrix4fv(vloc, 1, GL_TRUE, &view.m0);
    glUniformMatrix4fv(ploc, 1, GL_TRUE, &proj.m0);
    glUniform4f(cloc, 0.376, 0.102, 0.82, clampf(0.25, 1.0, 1.0f * sums[i]));

    glBindVertexArray(rd->VAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glLineWidth(2.0f);
    glUniform4f(cloc, 0.0, 0.0, 0.0, 1.0f);

    glDrawArrays(GL_TRIANGLES, 0, 36);

    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  }
  rangle = (rangle + 1.0f > 360.0f) ? rangle - 360.0f : rangle + 1.0f;
}

void sdl_gl_set_flags(void) {
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
}

void gl_viewport_update(SDL_Window *w, int *ww, int *wh) {
  SDL_GetWindowSize(w, ww, wh);
  glViewport(0, 0, *ww, *wh);
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
  Renderer_Data rd = {0, 0, 0, 0, 1};
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

  const float vertices[] = {
      -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f,
      0.5f,  0.5f,  -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, -0.5f, -0.5f,

      -0.5f, -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,  0.5f,  0.5f,
      0.5f,  0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,  -0.5f, -0.5f, 0.5f,

      -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  -0.5f, -0.5f, -0.5f, -0.5f,
      -0.5f, -0.5f, -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,  0.5f,

      0.5f,  0.5f,  0.5f,  0.5f,  0.5f,  -0.5f, 0.5f,  -0.5f, -0.5f,
      0.5f,  -0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,  0.5f,  0.5f,  0.5f,

      -0.5f, -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, 0.5f,  -0.5f, 0.5f,
      0.5f,  -0.5f, 0.5f,  -0.5f, -0.5f, 0.5f,  -0.5f, -0.5f, -0.5f,

      -0.5f, 0.5f,  -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,  0.5f,
      0.5f,  0.5f,  0.5f,  -0.5f, 0.5f,  0.5f,  -0.5f, 0.5f,  -0.5f,
  };

  glGenVertexArrays(1, &rd->VAO);

  glGenBuffers(1, &rd->VBO);

  glBindVertexArray(rd->VAO);

  glBindBuffer(GL_ARRAY_BUFFER, rd->VBO);
  // Data doesnt change
  glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

  glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);
  glEnableVertexAttribArray(0);
}
