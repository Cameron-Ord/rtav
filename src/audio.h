#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>
#include <stdint.h>

#define BUFFER_SIZE (1 << 13)

typedef struct
{
    int valid;
    float *buffer;
    uint32_t position;
    uint32_t len;
    size_t samples;
    size_t bytes;
    int channels;
    int format;
    int sr;
} AParams;

int get_audio_state(void);
void toggle_pause(void);
AParams *read_file(const char *fp);
int dev_from_data(AParams *data);
void _vol(float change);
void audio_start(void);
void audio_end(void);
int callback_check_pos(uint32_t len, uint32_t pos);
#endif
