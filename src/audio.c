#include <errno.h>
#include <string.h>

#include "audio.h"
#include <SDL2/SDL_audio.h>

#include <sndfile.h>

typedef struct
{
    const int bit;
    const char *str;
} mask_ret;

float vol = 1.0f;

SDL_AudioDeviceID dev;
SDL_AudioSpec spec = { 0 };

// The only exposed function in this file should be read_file(),
// toggle_pause() and _vol(), the state is handled internally in this src file

static int scmp(AParams *data);
static void callback(void *usrdata, unsigned char *stream, int len);
static void set_audio_spec(AParams *data);
static int open_device(void);
static const char *format_to_str(int format);
static float vclampf(float v);
static void vol_change_commit(float v);

// float root_mean_squared(const float *slice, const size_t size) {
// float sum = 0.0f;
// for (size_t i = 0; i < size; i++) {
// sum += slice[i] * slice[i];
//}
// return sqrtf(sum / size);
//}

void fft_push(const uint32_t bytes, const uint32_t offset, const float *srcbuf, float dstbuf[TWOBUFFER])
{
    if (bytes > 0 && (srcbuf && dstbuf)) {
        const size_t samples = bytes / sizeof(float);
        memmove(dstbuf, dstbuf + samples, bytes);
        memcpy(dstbuf + samples, srcbuf + offset, bytes);
    }
}

static float vclampf(const float v)
{
    if (v > 1.0) {
        return 1.0;
    }

    if (v < 0.0) {
        return 0.0;
    }

    return v;
}

static void vol_change_commit(const float v)
{
    if (v >= 0.0 && v <= 1.0) {
        vol = v;
    }
}

void _vol(const float change)
{
    return (vol + change >= 0.0 && vol + change <= 1.0)
               ? vol_change_commit(vol + change)
               : vol_change_commit(vclampf(vol + change));
}

int get_audio_state(void)
{
    if (dev) {
        return SDL_GetAudioDeviceStatus(dev);
    }
    return 0;
}

static void callback(void *usrdata, unsigned char *stream, int len)
{
    AParams *const p = (AParams *)usrdata;
    if (p && p->buffer && (len > 0 && stream)) {
        const uint32_t ulen = (uint32_t)len;
        const uint32_t samples = ulen / sizeof(p->buffer[0]);
        const uint32_t remaining = p->len - p->position;
        const uint32_t scount = (samples < remaining) ? samples : remaining;

        float *fstream = (float *)stream;
        // Every common audio file format (The ones im allowing to be
        // used) is spec'd to store its samples in interleaved format, so just pass
        // the data to stream as is.
        for (uint32_t i = 0; i < scount; i++) {
            if (i + p->position < p->len) {
                fstream[i] = p->buffer[i + p->position] * vol;
            }
        }

        if (p->position + scount <= p->len) {
            fft_push(ulen, p->position, p->buffer, p->sample_buffer);
            p->position += scount;
        }
    }
}

static int scmp(AParams *data)
{
    if (!spec.callback) {
        return 0;
    }

    if (spec.format != AUDIO_F32SYS) {
        return 0;
    }

    if (spec.channels != data->channels) {
        return 0;
    }

    if (spec.freq != data->sr) {
        return 0;
    }

    if (spec.samples != (1 << 13) / data->channels) {
        return 0;
    }

    return 1;
}

static void set_audio_spec(AParams *const data)
{
    spec.userdata = data;
    spec.callback = callback;
    spec.channels = data->channels;
    spec.freq = data->sr;
    spec.format = AUDIO_F32SYS;
    spec.samples = BUFFER_SIZE / data->channels;
}

static int open_device(void)
{
    dev = SDL_OpenAudioDevice(NULL, 0, &spec, NULL, 0);
    if (!dev) {
        printf("Could not open audio device : %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

// Device closing blocks until it is finished.
void close_device(void)
{
    if (dev) {
        SDL_ClearQueuedAudio(dev);
        SDL_CloseAudioDevice(dev);
        dev = 0;
    }
}

static int dstate(void)
{
    if (dev) {
        return SDL_GetAudioDeviceStatus(dev);
    }
    return 0;
}

void toggle_pause(void)
{
    if (dev) {
        switch (dstate()) {
        default:
            break;

        case SDL_AUDIO_PLAYING:
        {
            SDL_PauseAudioDevice(dev, SDL_TRUE);
        } break;

        case SDL_AUDIO_PAUSED:
        {
            SDL_PauseAudioDevice(dev, SDL_FALSE);
        } break;
        }
    }
}

void audio_start(void) { SDL_PauseAudioDevice(dev, SDL_FALSE); }
void audio_end(void) { SDL_PauseAudioDevice(dev, SDL_TRUE); }
static const char *format_to_str(const int format)
{
    const mask_ret fmasks[] = {
        { SF_FORMAT_WAV, "WAV" },
        { SF_FORMAT_FLAC, "FLAC" },
        { SF_FORMAT_AIFF, "AIFF" },
        { SF_FORMAT_MPEG, "MPEG" },
        { SF_FORMAT_OGG, "OGG" },
    };

    const int maskc = sizeof(fmasks) / sizeof(fmasks[0]);
    for (int i = 0; i < maskc; i++) {
        if ((format & SF_FORMAT_TYPEMASK) == fmasks[i].bit) {
            return fmasks[i].str;
        }
    }

    return "?";
}

// A data struct is allocated when created, needs to be freed when reading the
// next file.
AParams *read_file(const char *fp)
{
    if (!fp) {
        return NULL;
    }

    AParams *data = calloc(1, sizeof(AParams));
    if (!data) {
        printf("Could not allocate memory: %s\n", strerror(errno));
        return NULL;
    }

    data->valid = 0;
    SNDFILE *file = NULL;
    SF_INFO sfinfo = { 0 };

    printf("Reading: %s\n", fp);
    if (!(file = sf_open(fp, SFM_READ, &sfinfo))) {
        printf("Could not open file: %s - ERR: %s\n", fp,
               sf_strerror(NULL));
        return data;
    }

    if (sfinfo.channels != 2) {
        printf("File must have 2 channels\n");
        return data;
    }

    const size_t samples = sfinfo.frames * sfinfo.channels;
    const size_t bytes = samples * sizeof(float);

    printf("=======\n");
    printf("CHANNELS: %d\n", sfinfo.channels);
    printf("MAJOR FORMAT: %s\n", format_to_str(sfinfo.format));
    printf("SAMPLE RATE %d\n", sfinfo.samplerate);
    printf("BYTES %zu\n", bytes);
    printf("SAMPLES %zu\n", samples);
    printf("=======\n");

    float *tmp = NULL;
    if (!(tmp = calloc(samples, sizeof(float)))) {
        printf("Could not allocate buffer: %s\n", strerror(errno));
        sf_close(file);
        return data;
    }

    // If 0 bytes are read it mostly indicates theres nothing to read, but sndfile
    // will return -1 from this func if it attempts to read beyond the the end of
    // the file, and will not report an error in this case. Query for error
    // regardless.
    if (sf_read_float(file, tmp, samples) < 0) {
        printf("Error reading audio data: %s\n", sf_strerror(file));
        free(tmp);
        sf_close(file);
        return data;
    }
    sf_close(file);

    data->channels = sfinfo.channels;
    data->sr = sfinfo.samplerate;
    data->format = sfinfo.format;
    data->samples = samples;
    data->bytes = samples * sizeof(float);
    data->len = (uint32_t)data->samples;
    data->buffer = tmp;
    data->valid = 1;

    return data;
}

int dev_from_data(AParams *const data)
{
    // The id is not guaranteed to be any number and just is the number of
    // whatever SDL2 decided to use from internal ids of available devices, pretty
    // much guaranteed to be the default dev in use, but might want to ensure the
    // correct device is chosen.
    //
    // Anyway if the dev is 0 there is no device.
    switch (dev) {
    default:
    {
        close_device();
        set_audio_spec(data);
        return open_device();
    } break;

    case 0:
    {
        set_audio_spec(data);
        return open_device();
    } break;
    }
}

int callback_check_pos(const uint32_t len, const uint32_t pos)
{
    if (pos >= len) {
        return 0;
    } else if (pos <= len) {
        return 1;
    }
    return 0;
}
