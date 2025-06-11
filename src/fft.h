#ifndef FFT_H
#define FFT_H

#include <stddef.h>
typedef struct
{
    float real;
    float imag;
} Compf;

void gen_bins(int size);
float window(float in, float coeff);
void wfunc(float *in, const float *hamming, int size);
void calculate_window(float *hambuf);
void iter_fft(float *in, Compf *out, size_t size);
void compf_to_float(float *half, Compf *fft_output);
void section_bins(int sr, float *half, float *sums);
void interpolate(float *sums, float *ssmooth, float *ssmear, int frames);
#endif
