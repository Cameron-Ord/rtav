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

const float smin = 0.1;
const float smax = 4.0;
const float fdec = 0.99;
const float finc = 1.01;

typedef struct
{
    int x, y, z;
} iVec3;

typedef struct
{
    float x, y, z;
} fVec3;

const fVec3 bg = { 41.0 / 255, 44.0 / 255, 60.0 / 255 };
const fVec3 cube_sample = { 140.0 / 255, 170.0 / 255, 238.0 / 255 };
const fVec3 cube_smear = { 166.0 / 255, 209.0 / 255, 137.0 / 255 };
const fVec3 light = { 133.0 / 255, 193.0 / 255, 220.0 / 255 };

const size_t SHADER_SRC_MAX = 2048;
static int shader_src_fill(FILE *file, char *srcbuf);

typedef struct
{
    Matrix proj;
    Matrix view;
} MatObj;

typedef struct
{
    Matrix model;
} MatModel;

static MatObj load_mat_obj()
{
    MatObj mo = { ortho_mat(0, RENDER_WIDTH, 0, RENDER_HEIGHT, 0.1, 100.0),
                  identity() };
    return mo;
}

static void mat_model_scale(MatModel *const mo, const float model_width,
                            const float model_height)
{
    mo->model = multiply_mat(
        scale_mat(model_width, model_height, 5.0 /*LONG BIG LONG DEEP*/),
        mo->model);
}

static void mat_view_translate(MatObj *mo, const float x, const float y,
                               const float z)
{
    mo->view = multiply_mat(translate_mat(x, y, z), mo->view);
}

static void mat_model_translate_at(MatModel *const mo, const float xpos,
                                   const float ypos)
{
    mo->model = multiply_mat(translate_mat(xpos, ypos, 0.0), mo->model);
}

static void mat_model_rotate(MatModel *mo)
{
    static float angle;
    mo->model = multiply_mat(multiply_mat(rotate_matx(angle), rotate_maty(angle)),
                             mo->model);
    angle = (angle + 0.025f > 1.0f) ? angle - 1.0f : angle + 0.025;
}

static void gl_vertex_bind(const unsigned int VAO) { glBindVertexArray(VAO); }

static void gl_vertex_unbind(void) { glBindVertexArray(0); }

static void gl_prog_use(const unsigned int sid) { glUseProgram(sid); }

static void gl_draw_arrays(GLenum MODE, const int first, const int count)
{
    glDrawArrays(MODE, first, count);
}

static void gl_set_uniforms(const unsigned int sid, const MatModel *const mm,
                            const MatObj *const mo)
{

    unsigned int mloc = glGetUniformLocation(sid, "model");
    unsigned int vloc = glGetUniformLocation(sid, "view");
    unsigned int ploc = glGetUniformLocation(sid, "projection");
    // My matrix isnt laid out in memory like opengl expects it so transpose needs
    // to be true
    glUniformMatrix4fv(mloc, 1, GL_TRUE, &mm->model.m0);
    glUniformMatrix4fv(vloc, 1, GL_TRUE, &mo->view.m0);
    glUniformMatrix4fv(ploc, 1, GL_TRUE, &mo->proj.m0);
}
static void gl_set_obj_colour(const unsigned int sid, MatModel *const mm,
                              const fVec3 *const col)
{
    unsigned int lcloc = glGetUniformLocation(sid, "object_colour");
    glUniform3f(lcloc, col->x, col->y, col->z);
}

static void light_at(unsigned int sid, const float xpos, const float ypos,
                     const float zpos)
{
    unsigned int lploc = glGetUniformLocation(sid, "light_pos");
    unsigned int lcloc = glGetUniformLocation(sid, "light_colour");
    unsigned int vploc = glGetUniformLocation(sid, "view_pos");

    glUniform3f(lcloc, light.x, light.y, light.z);
    glUniform3f(lploc, xpos, ypos, zpos);
    glUniform3f(vploc, (float)RENDER_WIDTH / 2, (float)RENDER_HEIGHT / 2, 10.0);
}

static void gl_uniform_and_draw(const unsigned int sid, const MatObj *const mo,
                                const MatModel *const mm,
                                const unsigned int VAO)
{
    gl_set_uniforms(sid, mm, mo);
    gl_vertex_bind(VAO);
    gl_draw_arrays(GL_TRIANGLES, 0, 6);
    gl_vertex_unbind();
}

void gl_draw_buffer(Renderer_Data *rd, const float *smthframes,
                    const float *smrframes)
{
    const unsigned int sid = rd->shader_program_id;
    gl_prog_use(sid);

    const float model_width = (float)RENDER_WIDTH / DIVISOR;
    const float model_height = (float)RENDER_HEIGHT / DIVISOR;

    MatObj mo = load_mat_obj();
    mat_view_translate(&mo, 0.0, 0.0, -10.0);
    const float wspacing = model_width / 4;
    const float hspacing = model_height / 4;

    for (int i = 0; i < DIVISOR; i++) {
        const float xpos = i * model_width + wspacing / 2;

        const int crow = roundf(smthframes[i] * DIVISOR);
        const int frow = roundf(smrframes[i] * DIVISOR);

        if (crow == 0 && frow == 0) {
            continue;
        }

        const int ceil_grid_loc = crow * model_height;
        const int fall_grid_loc = frow * model_height;

        light_at(sid, xpos, ((crow + 1) * model_height), 50);

        if (frow > crow) {
            int j = frow;
            while (j > crow && j > 0) {
                MatModel mm = { identity() };
                const int cube_y = j * model_height + hspacing / 2;

                gl_set_obj_colour(sid, &mm, &cube_smear);
                mat_model_scale(&mm, model_width - wspacing, model_height - hspacing);
                mat_model_translate_at(&mm, xpos, cube_y);

                gl_uniform_and_draw(sid, &mo, &mm, rd->VAO);
                j--;
            }
        }

        int j = 0;
        while (j <= crow) {
            MatModel mm = { identity() };
            const int cube_y = j * model_height;

            gl_set_obj_colour(sid, &mm, &cube_sample);
            mat_model_scale(&mm, model_width - wspacing, model_height);
            mat_model_translate_at(&mm, xpos, cube_y);

            gl_uniform_and_draw(sid, &mo, &mm, rd->VAO);
            j++;
        }
    }
}

static int shader_src_fill(FILE *file, char *srcbuf)
{
    int i = 0;
    if (file && srcbuf) {
        memset(srcbuf, 0, SHADER_SRC_MAX * sizeof(char));
        while (fread(&srcbuf[i], 1, 1, file) > 0 && i < SHADER_SRC_MAX) {
            i++;
        }
        srcbuf[i] = '\0';
        printf("\nLOADED SHADER:\n%s\n", srcbuf);
        if (!fclose(file)) {
            printf("Could not close file: %s\n", strerror(errno));
        }
    }
    return i;
}

static float clampf(const float min, const float max, const float sample)
{
    if (sample < min) {
        return min;
    }

    if (sample > max) {
        return max;
    }

    return sample;
}

// Todo make each sample a cube, and use FFT

void sdl_gl_set_flags(void)
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
}

void gl_clear_canvas(void)
{
    glClearColor(bg.x, bg.y, bg.z, 1.0);
    glClear(GL_COLOR_BUFFER_BIT);
}

static int will_fit(const float scale)
{
    return (int)(RENDER_HEIGHT * scale) % DIVISOR == 0 &&
           (int)(RENDER_WIDTH * scale) % DIVISOR == 0;
}

static int w_greater(const float scale, const float factor, const int w)
{
    return RENDER_WIDTH * (scale * factor) > w;
}

static int h_greater(const float scale, const float factor, const int h)
{
    return RENDER_HEIGHT * (scale * factor) > h;
}

static int or_greater(const float scale, const float factor, const int w,
                      const int h)
{
    return w_greater(scale, factor, w) || h_greater(scale, factor, h);
}

static int both_greater(const float scale, const float factor, const int w,
                        const int h)
{
    return w_greater(scale, factor, w) && h_greater(scale, factor, h);
}

static float resize_query(const int w, const int h)
{
    float scale = 1.0f;
    if (w < RENDER_WIDTH || h < RENDER_HEIGHT) {
        while (scale > smin && or_greater(scale, fdec, w, h)) {
            scale *= fdec;
        }
    } else if (w > RENDER_WIDTH || h > RENDER_HEIGHT) {
        while (scale < smax && !both_greater(scale, finc, w, h)) {
            scale *= finc;
        }
    }
    return scale;
}

void gl_viewport_update(SDL_Window *w, int *ww, int *wh)
{
    SDL_GetWindowSize(w, ww, wh);
    // Todo: scale this by multiples/divisions of the base render size
    // dynamically
    const float scale = resize_query(*ww, *wh);

    const int basew = RENDER_WIDTH * scale;
    const int baseh = RENDER_HEIGHT * scale;

    const int x = (*ww - basew) * 0.5;
    const int y = (*wh - baseh) * 0.5;
    glViewport(x, y, basew, baseh);
}

int check_link_state(const unsigned int *program_id)
{
    int status;
    char log[512];
    glGetProgramiv(*program_id, GL_LINK_STATUS, &status);
    if (!status) {
        glGetProgramInfoLog(*program_id, 512, NULL, log);
        printf("Shader linking failed : %s\n", log);
        return 0;
    }
    return 1;
}

FILE *open_shader_src(const char *path, const char *fn)
{
    const size_t spathlen = strlen(path);
    const size_t fnlen = strlen(fn);

    char *fullpath = malloc(spathlen + fnlen + 1);
    if (!fullpath) {
        printf("Could not allocate memory: %s\n", strerror(errno));
        return NULL;
    }
    memset(fullpath, 0, spathlen + fnlen);

    strncpy(fullpath, path, spathlen);
    strncat(fullpath, fn, fnlen);

    fullpath[spathlen + fnlen] = '\0';

    printf("Opening: %s of path %s\n", fn, path);
    FILE *file = fopen(fullpath, "r");
    if (!file) {
        printf("Could not open file: %s\n", strerror(errno));
        return NULL;
    }

    free(fullpath);
    return file;
}

Renderer_Data load_shaders(void)
{
    // Assume it's failed until its proven otherwise
    Renderer_Data rd = {
        0, 0, 0, 0, 1, (float)RENDER_WIDTH / 2, (float)RENDER_HEIGHT / 2, 0.0
    };
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
        printf("Empty shader src\n");
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

unsigned int attach_shaders(const unsigned int *fs, const unsigned int *vs)
{
    unsigned int program = glCreateProgram();
    glAttachShader(program, *fs);
    glAttachShader(program, *vs);
    glLinkProgram(program);
    return program;
}

int compile_shader(const char *src, unsigned int *shader, const unsigned int type)
{
    const GLchar *glsrc = src;

    *shader = glCreateShader(type);
    glShaderSource(*shader, 1, &glsrc, NULL);
    glCompileShader(*shader);

    int result;
    char log[512];

    glGetShaderiv(*shader, GL_COMPILE_STATUS, &result);
    if (!result) {
        glGetShaderInfoLog(*shader, 512, NULL, log);
        printf("Shader compilation failed: %s\n", log);
        return 0;
    }

    return 1;
}

void gl_data_construct(Renderer_Data *rd)
{
    // 1.618 golden ratio
    // 12 vertices 20 faces icosahedron
    // const float g = (1.0 + sqrt(5.0)) / 2.0;
    // const float scaled = 1.0 / sqrt(1.0 + g * g);
    // const float x = scaled;
    // const float y = g * scaled;
    //

    float vertices[] = {
        // Position         // Normal
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Bottom-left
        1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Bottom-right
        1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Top-right

        1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Top-right
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, // Top-left
        0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f  // Bottom-left
    };

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
