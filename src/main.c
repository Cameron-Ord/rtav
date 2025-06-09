#include <GL/glew.h>
#include <SDL2/SDL_timer.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio.h"
#include "entry.h"
#include "fft.h"
#include "renderer.h"
#include "rndrdef.h"

#include <GL/gl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <time.h>

static SDL_Window *make_window(void);
static AParams *begin_audio_file(const Entry *const e);
static AParams *__begin_bad(AParams *p);
static AParams *__begin_ok(AParams *p);
static int query_audio_position(AParams **p);
static AParams *_next(int *attempts, Entry *current);
static void *free_params(AParams *p);
static void _fail(int *run, const int attempts);
static uint32_t _scount(const uint32_t remaining);

int main(int argc, char **argv) {
  srand(time(NULL));
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
  glEnable(GL_BLEND);

  Renderer_Data rd = load_shaders();
  if (rd.broken) {
    return 1;
  }
  gl_data_construct(&rd);

  Entry *const estart = ents.list;
  Entry *const eend = ents.list + ents.size;
  Entry *current = ents.list;
  AParams *p = begin_audio_file(current);

  float hambuf[BUFFER_SIZE];
  memset(hambuf, 0, sizeof(float) * BUFFER_SIZE);
  calculate_window(hambuf);

  SDL_ShowWindow(win);

  const int MAX_ATTEMPTS = 6;
  int song_queued = 0, attempts = 0;
  int run = 1;

  float sample_buffer[BUFFER_SIZE];
  Compf out_buffer[BUFFER_SIZE];
  float out_half[BUFFER_SIZE / 2];
  float sums[DIVISOR];
  float ssmooth[DIVISOR];
  memset(ssmooth, 0, sizeof(float) * DIVISOR);

  uint32_t lastinput = SDL_GetTicks64();

  while (run) {
    const uint32_t start = SDL_GetTicks64();

    song_queued = query_audio_position(&p);
    if (song_queued && p) {
      p = free_params(p);
      current = (current + 1 != eend) ? current + 1 : estart;
      memset(ssmooth, 0, sizeof(float) * DIVISOR);
      p = _next(&attempts, current);

    } else if (!song_queued && !p) {
      current = (current + 1 != eend) ? current + 1 : estart;
      memset(ssmooth, 0, sizeof(float) * DIVISOR);
      if (!(p = _next(&attempts, current)) && attempts > MAX_ATTEMPTS) {
        _fail(&run, attempts);
      }
    }

    memset(out_half, 0, sizeof(float) * BUFFER_SIZE / 2);
    memset(out_buffer, 0, sizeof(Compf) * BUFFER_SIZE);

    if (p && p->buffer && get_audio_state() == SDL_AUDIO_PLAYING) {
      const uint32_t remaining = p->len - p->position;
      const uint32_t scount = _scount(remaining);
      const float *const buffer_at = p->buffer + p->position;
      memcpy(sample_buffer, buffer_at, scount * sizeof(float));

      iter_fft(sample_buffer, hambuf, out_buffer, BUFFER_SIZE);
      compf_to_float(out_half, out_buffer);
      memset(sums, 0, sizeof(float) * DIVISOR);
      section_bins(p->sr, out_half, sums);
      interpolate(sums, ssmooth, 8, 60);
    }

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
      default:
        break;

      case SDL_KEYDOWN: {
        int keysym = e.key.keysym.sym;
        switch (keysym) {

        case SDLK_s: {
          if (rd.lightz + 1.0f < 100.0) {
            rd.lightz += 1.0f;
          }
        } break;

        case SDLK_d: {
          if (rd.lightz + -1.0f > 0.1) {
            rd.lightz += -1.0f;
          }
        } break;

        case SDLK_DOWN: {
        } break;

        case SDLK_UP: {
        } break;

        case SDLK_LEFT: {
          float cd = 250;
          if (SDL_GetTicks64() - lastinput >= cd) {
            audio_end();
            p = free_params(p);
            current = (current > estart) ? current - 1 : eend - 1;
            memset(ssmooth, 0, sizeof(float) * DIVISOR);
            p = _next(&attempts, current);
            lastinput = SDL_GetTicks64();
          }
        } break;

        case SDLK_RIGHT: {
          float cd = 250;
          if (SDL_GetTicks64() - lastinput >= cd) {
            audio_end();
            p = free_params(p);
            current = (current + 1 < eend) ? current + 1 : estart;
            memset(ssmooth, 0, sizeof(float) * DIVISOR);
            p = _next(&attempts, current);
            lastinput = SDL_GetTicks64();
          }
        } break;
        }
      } break;

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

    gl_draw_buffer(&rd, ssmooth, ww, wh);
    SDL_GL_SwapWindow(win);

    const uint32_t duration = SDL_GetTicks64() - start;
    const uint32_t delta = 1000 / 60;

    if (duration < delta) {
      SDL_Delay(delta - duration);
    }
  }

  return 0;
}

static SDL_Window *make_window(void) {
  const int flags =
      SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
  SDL_Window *win =
      SDL_CreateWindow("3DMV", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                       RENDER_WIDTH, RENDER_HEIGHT, flags);
  return win;
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

static int query_audio_position(AParams **p) {
  if ((p && *p) && !callback_check_pos((*p)->len, (*p)->position)) {
    audio_end();
    return 1;
  } else {
    return 0;
  }
}

static AParams *_next(int *attempts, Entry *current) {
  AParams *p = begin_audio_file(current);
  int cond = p != NULL;
  switch (cond) {
  default: {
    *attempts = 0;
    return p;
  } break;

  case 0: {
    *attempts += 1;
    return NULL;
  } break;
  }
}

static void _fail(int *run, const int attempts) {
  *run = 0;
  fprintf(stderr, "Gave up after %d attempts to load a file\n", attempts);
}

static uint32_t _scount(const uint32_t remaining) {
  return (BUFFER_SIZE < remaining) ? BUFFER_SIZE : remaining;
}
