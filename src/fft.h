#ifndef FFT_H
#define FFT_H

#include <stddef.h>
typedef struct {
  float real;
  float imag;
} Compf;

float window(const float in, const float coeff);
void calculate_window(float *hambuf);
void iter_fft(float *in, float *hambuf, Compf *out, size_t size);
void compf_to_float(float *half, Compf *fft_output);
void section_bins(const int sr, float *half, float *sums);
#endif
