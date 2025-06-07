#ifndef AUDIO_H
#define AUDIO_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
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

void toggle_pause(void);
AParams *read_file(const char *fp);
int dev_from_data(AParams *const data);
void audio_start(void);
void audio_end(void);
int callback_check_pos(const uint32_t len, const uint32_t pos);
#endif
