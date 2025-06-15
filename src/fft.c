#include "fft.h"
#include "audio.h"
#include "rndrdef.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

const int smear = 8;
const int smooth = 8;
float bins[DIVISOR + 1];

const float MAX_FREQ = 5000.0f;
const float MIN_FREQ = 60.0f;
const float RATIO = MAX_FREQ / MIN_FREQ;

void ema(const float *new, float *old)
{
    const float a = 0.1;
    for (int i = 0; i < DIVISOR; i++) {
        old[i] = (a * new[i]) + ((1.0 - a) * old[i]);
    }
}

static inline Compf c_from_real(const float real)
{
    Compf _complex;
    _complex.real = real;
    _complex.imag = 0.0;
    return _complex;
}

static inline Compf c_from_imag(const float imag)
{
    Compf _complex;
    _complex.real = 0.0;
    _complex.imag = imag;
    return _complex;
}

static inline Compf compf_expf(const Compf *c)
{
    Compf res;
    float exp_real = expf(c->real);
    res.real = exp_real * cosf(c->imag);
    res.imag = exp_real * sinf(c->imag);
    return res;
}

static inline Compf compf_subtract(const Compf *a, const Compf *b)
{
    Compf sub;
    sub.real = a->real - b->real;
    sub.imag = a->imag - b->imag;
    return sub;
}

static inline Compf compf_add(const Compf *a, const Compf *b)
{
    Compf add;
    add.real = a->real + b->real;
    add.imag = a->imag + b->imag;
    return add;
}

static inline Compf compf_mult(const Compf *a, const Compf *b)
{
    Compf mult;
    mult.real = a->real * b->real - a->imag * b->imag;
    mult.imag = a->real * b->imag + a->imag * b->real;
    return mult;
}

static inline Compf compf_step(const size_t *half_len, const Compf *iota)
{
    Compf step;
    float theta = (float)M_PI / *half_len;

    step.real = iota->real * theta;
    step.imag = iota->imag * theta;

    step = compf_expf(&step);
    return step;
}

static inline size_t bit_reverse(size_t index, size_t log2n)
{
    size_t reversed = 0;
    for (size_t i = 0; i < log2n; i++) {
        reversed <<= 1;
        reversed |= (index & 1);
        index >>= 1;
    }
    return reversed;
}

void iter_fft(float *in, Compf *out, size_t size)
{
    for (size_t i = 0; i < size; i++) {
        int rev_index = bit_reverse(i, log2(size));
        out[i] = c_from_real(in[rev_index]);
    }

    const Compf iota = c_from_imag(1.0f);
    for (size_t stage = 1; stage <= log2(size); ++stage) {
        size_t sub_arr_size = (size_t)1 << stage; // 2^stage
        size_t half_sub_arr = sub_arr_size >> 1;
        Compf twiddle = c_from_real(1.0f);

        Compf step = compf_step(&half_sub_arr, &iota);
        for (size_t j = 0; j < half_sub_arr; j++) {
            for (size_t k = j; k < size; k += sub_arr_size) {
                Compf t = compf_mult(&twiddle, &out[k + half_sub_arr]);
                Compf u = out[k];

                out[k] = compf_add(&u, &t);
                out[k + half_sub_arr] = compf_subtract(&u, &t);
            }
            twiddle = compf_mult(&twiddle, &step);
        }
    }
}

float window(const float in, const float coeff) { return in * coeff; }

void wfunc(float *in, const float *hamming, const int size)
{
    for (int i = 0; i < size; i++) {
        in[i] = window(in[i], hamming[i]);
    }
}

void calculate_window(float *hambuf)
{
    const float PI = 3.14159265359f;
    for (int i = 0; i < BUFFER_SIZE; ++i) {
        float t = (float)i / (BUFFER_SIZE - 1);
        hambuf[i] = 0.54 - 0.46 * cosf(2 * PI * t);
    }
}

void compf_to_float(float *half, Compf *fft_output)
{
    const size_t half_size = BUFFER_SIZE / 2;
    for (size_t i = 0; i < half_size; i++) {
        const Compf *const c = &fft_output[i];
        half[i] = sqrtf(c->real * c->real + c->imag * c->imag);
    }
}

void gen_bins(const int size)
{
    for (int i = 0; i < size; i++) {
        float t = (float)i / (size - 1);
        // BASE * POSITION
        // 20 * 1000^0.95 = 14158..
        // 20 * 1000^0.98 = 17419..
        // 20 * 1000^1 = 20000
        float k = MIN_FREQ * powf(RATIO, t);
        bins[i] = k;
    }

    printf("\n====BINS====\n");
    int newline = 0;
    for (int k = 0; k < size; k++) {
        printf("%.3f ", bins[k]);
        newline++;
        if (newline >= DIVISOR / 4) {
            printf("\n");
            newline = 0;
        }
    }
    printf("\n====BINS====\n");
}

static void bin_slice(const float freq, const float s, float *sums,
                      float *max)
{
    for (int j = 0; j < DIVISOR; j++) {
        if (freq >= bins[j] && freq < bins[j + 1]) {
            if (s > sums[j]) {
                sums[j] = s;
            }

            if (sums[j] > *max) {
                *max = sums[j];
            }
            return;
        }
    }
}

void section_bins(const int sr, float *half, float *sums)
{
    const int half_size = BUFFER_SIZE / 2;
    float max = half[0];
    for (int i = 0; i < half_size && i * ((float)sr / BUFFER_SIZE) < MAX_FREQ; i++) {
        const float freq = i * ((float)sr / BUFFER_SIZE);
        bin_slice(freq, half[i], sums, &max);
    }

    for (int l = 0; l < DIVISOR; l++) {
        if (sums[l] > 0.0) {
            sums[l] /= max;
        }
    }
}

static float ls(float base, float sm, int amt, int frames)
{
    return (base - sm) * amt * (1.0 / frames);
}

void interpolate(float *sums, float *ssmooth, float *ssmear, const int frames)
{
    for (int i = 0; i < DIVISOR; i++) {
        ssmooth[i] += ls(sums[i], ssmooth[i], smooth, frames);
        ssmear[i] += ls(ssmooth[i], ssmear[i], smear, frames);
    }
}
