#include <GL/glew.h>
#include <SDL2/SDL_timer.h>
#include <SDL2/SDL_video.h>
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
#include <assert.h>
#include <errno.h>
#include <time.h>

typedef struct
{
    Compf out_buffer[BUFFER_SIZE];
    float out_half[BUFFER_SIZE];
} Raw;

typedef struct
{
    float sums[DIVISOR];
    float ssmooth[DIVISOR];
    float ssmear[DIVISOR];
} Transformed;

static SDL_Window *make_window(const char *argv);
static AParams *begin_audio_file(const Entry *e);
static AParams *__begin_bad(AParams *p);
static AParams *__begin_ok(AParams *p);
static int query_audio_position(AParams **p);
static AParams *prepare_next(int *attempts, const Entry *current);
static AParams *find_queued(int *attempts, const Entry **current, const Entry *estart, const Entry *eend, int dir);
static void *free_params(AParams *p);
static void _fail(int *run, int attempts);
static uint32_t _scount(uint32_t remaining);
static void wipe(Transformed *tf, Raw *raw);

int main(int argc, char **argv)
{
    srand(time(NULL));
    if (argc < 2 || argc > 2) {
        printf("Usage: ./3dmv <directory>\n");
        return 0;
    }

    const char *directory = argv[1];
    Entries ents = read_directory(directory);

    if (ents.size == 0) {
        printf("Empty/Non-existant diretory\n");
        return 0;
    }

    // Todo : cleanup if this happens.
    if (ents.malformed) {
        if (ents.list) {
            free(ents.list);
        }
        printf("Error occured while reading dir entries\n");
        return 1;
    }

    if (parse_headers(&ents) == 0) {
        printf("Directory contains no audio files\n");
        return 1;
    }
    printf("%s\n", SHADER_PATH);

    if (SDL_Init(SDL_INIT_AUDIO | SDL_INIT_EVENTS | SDL_INIT_VIDEO) < 0) {
        printf("Could not initialize SDL2: %s\n", SDL_GetError());
        return 1;
    }
    sdl_gl_set_flags();

    SDL_Window *const win = make_window(directory);
    if (!win) {
        printf("Failed to create window : %s\n", SDL_GetError());
        return 1;
    }

    SDL_GLContext *const glcontext = SDL_GL_CreateContext(win);
    if (!glcontext) {
        printf("Failed to create OpenGL context : %s\n", SDL_GetError());
        return 1;
    }

    if (glewInit() != GLEW_OK) {
        printf("Failed to initialize libGLEW\n");
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
    const Entry *current = estart;
    AParams *p = begin_audio_file(current);

    float hambuf[BUFFER_SIZE];
    Raw raw = { 0 };
    Transformed tf = { 0 };

    calculate_window(hambuf);
    gen_bins(DIVISOR + 1);

    const int MAX_ATTEMPTS = 6;
    int song_queued = 0, attempts = 0;
    int run = 1;

    uint32_t lastinput = SDL_GetTicks64();
    SDL_EnableScreenSaver();
    SDL_ShowWindow(win);

    while (run) {
        const uint32_t start = SDL_GetTicks64();
        gl_clear_canvas();

        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
            default:
                break;

            case SDL_KEYDOWN:
            {
                int keysym = e.key.keysym.sym;
                switch (keysym) {

                case SDLK_p:
                {
                    toggle_pause();
                } break;

                case SDLK_DOWN:
                {
                    _vol(-0.1);
                } break;

                case SDLK_UP:
                {
                    _vol(0.1);
                } break;

                case SDLK_LEFT:
                {
                    float cd = 100;
                    if (SDL_GetTicks64() - lastinput >= cd) {
                        audio_end();
                        wipe(&tf, &raw);
                        AParams *tmp = p;
                        p = find_queued(&attempts, &current, estart, eend, -1);
                        tmp = free_params(tmp);
                        lastinput = SDL_GetTicks64();
                    }
                } break;

                case SDLK_RIGHT:
                {
                    float cd = 100;
                    if (SDL_GetTicks64() - lastinput >= cd) {
                        audio_end();
                        wipe(&tf, &raw);
                        AParams *tmp = p;
                        p = find_queued(&attempts, &current, estart, eend, 1);
                        tmp = free_params(tmp);
                        lastinput = SDL_GetTicks64();
                    }
                } break;
                }
            } break;

            case SDL_WINDOWEVENT:
            {
                switch (e.window.event) {
                case SDL_WINDOWEVENT_RESIZED:
                {
                    gl_viewport_update(win, &ww, &wh);
                } break;
                case SDL_WINDOWEVENT_SIZE_CHANGED:
                {
                    gl_viewport_update(win, &ww, &wh);
                } break;
                }
            } break;
            case SDL_QUIT:
            {
                run = 0;
            } break;
            }
        }

        song_queued = query_audio_position(&p);
        if (song_queued && p) {
            audio_end();
            wipe(&tf, &raw);
            AParams *tmp = p;
            p = find_queued(&attempts, &current, estart, eend, 1);
            tmp = free_params(tmp);

        } else if (!song_queued && !p) {
            audio_end();
            wipe(&tf, &raw);
            p = find_queued(&attempts, &current, estart, eend, 1);
            if (!p && attempts > MAX_ATTEMPTS) {
                _fail(&run, attempts);
            }
        }

        if (p && p->buffer && get_audio_state() == SDL_AUDIO_PLAYING) {
            float snapshot[BUFFER_SIZE];
            memset(&raw, 0, sizeof(Raw));
            memset(tf.sums, 0, sizeof(float) * DIVISOR);
            memcpy(snapshot, p->sample_buffer, sizeof(float) * BUFFER_SIZE);

            wfunc(snapshot, hambuf, BUFFER_SIZE);
            iter_fft(snapshot, raw.out_buffer, BUFFER_SIZE);
            compf_to_float(raw.out_half, raw.out_buffer);
            section_bins(p->sr, raw.out_half, tf.sums);
            interpolate(tf.sums, tf.ssmooth, tf.ssmear, 60);
        }

        gl_draw_buffer(&rd, tf.ssmooth, tf.ssmear);
        SDL_GL_SwapWindow(win);

        const uint32_t duration = SDL_GetTicks64() - start;
        const uint32_t delta = 1000 / 60;

        if (duration < delta) {
            SDL_Delay(delta - duration);
        }
    }

    // Close before freeing params - if there is queued audio after free that would be be bad
    audio_end();
    close_device();

    p = free_params(p);
    if (ents.list) {
        free(ents.list);
    }

    glDeleteProgram(rd.shader_program_id);
    glDeleteBuffers(1, &rd.VBO);
    glDeleteVertexArrays(1, &rd.VAO);

    SDL_GL_DeleteContext(glcontext);
    SDL_DestroyWindow(win);
    SDL_Quit();

    return 0;
}

static SDL_Window *make_window(const char *argv)
{

    const char *nameptr = NULL;
    const char *win_name = "rtav";

    const size_t arg_len = strlen(argv);
    const size_t name_len = strlen(win_name);
    // + 2 for space and NULL terminator
    const size_t buffer_len = arg_len + name_len + 2;

    char namebuffer[buffer_len];
    memset(namebuffer, 0, sizeof(char) * buffer_len);

    if (!snprintf(namebuffer, buffer_len, "%s %s", win_name, argv)) {
        printf("Failed to concatenate: %s\n", strerror(errno));
        nameptr = win_name;
    } else {
        nameptr = namebuffer;
    }

    const int flags =
        SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE;
    SDL_Window *win =
        SDL_CreateWindow(nameptr, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                         RENDER_WIDTH, RENDER_HEIGHT, flags);
    return win;
}

static AParams *__begin_bad(AParams *p)
{
    const int cond = p != NULL;
    switch (cond) {
    default:
    {
        return NULL;
    } break;

    case 1:
    {
        return free_params(p);
    } break;
    }
}

static AParams *__begin_ok(AParams *p)
{
    audio_start();
    return p;
}

static AParams *begin_audio_file(const Entry *e)
{
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

static void *free_params(AParams *p)
{
    if (p) {
        if (p->buffer) {
            free(p->buffer);
        }

        free(p);
    }
    return NULL;
}

static int query_audio_position(AParams **p)
{
    if ((p && *p) && !callback_check_pos((*p)->len, (*p)->position)) {
        audio_end();
        return 1;
    } else {
        return 0;
    }
}

static AParams *prepare_next(int *attempts, const Entry *const current)
{
    AParams *p = begin_audio_file(current);
    int cond = p != NULL;
    switch (cond) {
    default:
    {
        *attempts = 0;
        return p;
    } break;

    case 0:
    {
        *attempts += 1;
        return NULL;
    } break;
    }
}

static void _fail(int *run, const int attempts)
{
    *run = 0;
    printf("Gave up after %d attempts to load a file\n", attempts);
}

static uint32_t _scount(const uint32_t remaining)
{
    return (BUFFER_SIZE < remaining) ? BUFFER_SIZE : remaining;
}

static void wipe(Transformed *tf, Raw *raw)
{
    memset(tf, 0, sizeof(Transformed));
    memset(raw, 0, sizeof(Raw));
}

static AParams *find_queued(int *attempts, const Entry **current, const Entry *const estart, const Entry *eend, const int dir)
{
    if ((current && *current) && estart && eend) {
        switch (dir) {
        case 1:
        {
            *current = (*current + dir < eend) ? *current + dir : estart;
        } break;
        case -1:
        {
            *current = (*current + dir >= estart) ? *current + dir : eend - 1;
        } break;
        }

        if (!(*current)->is_audio_file) {
            while ((*current >= estart && *current + 1 < eend) && !(*current)->is_audio_file) {
                (*current)++;
            }
        }
        assert(*current >= estart && *current < eend);
        return prepare_next(attempts, *current);
    }
    return NULL;
}
