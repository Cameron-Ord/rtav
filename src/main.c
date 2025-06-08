#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>

#include "audio.h"
#include "entry.h"
#include "fft.h"
#include "renderer.h"

#include <GL/gl.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <time.h>

const int BASE_WIDTH = 400;
const int BASE_HEIGHT = 300;

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
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

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

  while (run) {
    const uint32_t start = SDL_GetTicks64();

    song_queued = query_audio_position(&p);
    if (song_queued && !p) {
      current = (current + 1 != eend) ? current + 1 : estart;
      p = _next(&attempts, current);

    } else if (!song_queued && !p) {
      current = (current + 1 != eend) ? current + 1 : estart;
      if (!(p = _next(&attempts, current)) && attempts > MAX_ATTEMPTS) {
        _fail(&run, attempts);
      }
    }

    // Obviously this is subject to change
    float sample_buffer[BUFFER_SIZE];
    Compf out_buffer[BUFFER_SIZE];
    float out_half[BUFFER_SIZE / 2];

    memset(sample_buffer, 0, BUFFER_SIZE * sizeof(float));
    memset(out_half, 0, (BUFFER_SIZE / 2) * sizeof(float));
    memset(out_buffer, 0, BUFFER_SIZE * sizeof(Compf));

    if (p && p->buffer && get_audio_state() == SDL_AUDIO_PLAYING) {
      const uint32_t remaining = p->len - p->position;
      const uint32_t scount = _scount(remaining);
      const float *const buffer_at = p->buffer + p->position;
      memcpy(sample_buffer, buffer_at, scount * sizeof(float));
      iter_fft(sample_buffer, hambuf, out_buffer, BUFFER_SIZE);

      for (size_t i = 0; i < BUFFER_SIZE / 2; i++) {
        const Compf *const c = &out_buffer[i];
        out_half[i] = sqrtf(c->real * c->real + c->imag * c->imag);
      }
      const int bin_count = 10;
      // 11 elements with the last value being the sentinel
      const float bins[] = {20,   40,   80,   160,   320,  640,
                            1280, 2560, 5120, 10240, 20480};
      float sums[bin_count];
      memset(sums, 0, sizeof(float) * bin_count);

      for (int i = 0; i < BUFFER_SIZE / 2; i++) {
        float freq = i * (float)p->sr / BUFFER_SIZE;
        for (int j = 0; j < bin_count; j++) {
          if (freq >= bins[j] && freq < bins[j + 1]) {
            sums[j] += out_half[i];
            break;
          }
        }
      }

      printf("==BIN BEGIN==\n");
      for (int i = 0; i < bin_count; i++) {
        printf("%.3f\n", sums[i]);
      }
      printf("==BIN END==\n");
    }

    glClearColor(0.0, 0.0, 0.0, 0.0);
    glClear(GL_COLOR_BUFFER_BIT);

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

    gl_draw_buffer(&rd, 0.0, ww, wh);
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
                       BASE_WIDTH, BASE_HEIGHT, flags);
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
    *p = free_params(*p);
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
