#ifndef RENDERER_H
#define RENDERER_H

typedef struct _IO_FILE FILE;
typedef struct SDL_Window SDL_Window;
typedef struct {
  unsigned int VBO, VAO, EBO;
  unsigned int shader_program_id;
  int broken;
} Renderer_Data;

void gl_draw_buffer(Renderer_Data *rd, const float *sums, const int bcount,
                    const int ww, const int wh);
void sdl_gl_set_flags(void);
void gl_viewport_update(SDL_Window *w, int *ww, int *wh);
int check_link_state(const unsigned int *program);
FILE *open_shader_src(const char *path, const char *fn);
Renderer_Data load_shaders(void);
unsigned int attach_shaders(unsigned int *fs, unsigned int *vs);
int compile_shader(const char *src, unsigned int *shader, unsigned int type);
void gl_data_construct(Renderer_Data *rd);
#endif
