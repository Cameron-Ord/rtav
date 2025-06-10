#ifndef FFT_H
#define FFT_H

#include <stddef.h>
typedef struct {
  float real;
  float imag;
} Compf;

void gen_bins(const int size);
float window(const float in, const float coeff);
void wfunc(float *in, const float *hambuf, const int size);
void calculate_window(float *hambuf);
void iter_fft(float *in, Compf *out, size_t size);
void compf_to_float(float *half, Compf *fft_output);
void section_bins(const int sr, float *half, float *sums);
void interpolate(float *sums, float *ssmooth, float *ssmear, const int frames);
#endif
