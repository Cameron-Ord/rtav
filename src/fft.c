#include "fft.h"
#include "audio.h"
#include "rndrdef.h"
#include <math.h>
#include <stdio.h>

static inline Compf c_from_real(const float real) {
  Compf _complex;
  _complex.real = real;
  _complex.imag = 0.0;
  return _complex;
}

static inline Compf c_from_imag(const float imag) {
  Compf _complex;
  _complex.real = 0.0;
  _complex.imag = imag;
  return _complex;
}

static inline Compf compf_expf(const Compf *c) {
  Compf res;
  float exp_real = expf(c->real);
  res.real = exp_real * cosf(c->imag);
  res.imag = exp_real * sinf(c->imag);
  return res;
}

static inline Compf compf_subtract(const Compf *a, const Compf *b) {
  Compf sub;
  sub.real = a->real - b->real;
  sub.imag = a->imag - b->imag;
  return sub;
}

static inline Compf compf_add(const Compf *a, const Compf *b) {
  Compf add;
  add.real = a->real + b->real;
  add.imag = a->imag + b->imag;
  return add;
}

static inline Compf compf_mult(const Compf *a, const Compf *b) {
  Compf mult;
  mult.real = a->real * b->real - a->imag * b->imag;
  mult.imag = a->real * b->imag + a->imag * b->real;
  return mult;
}

static inline Compf compf_step(const size_t *half_len, const Compf *iota) {
  Compf step;
  float theta = (float)M_PI / *half_len;

  step.real = iota->real * theta;
  step.imag = iota->imag * theta;

  step = compf_expf(&step);
  return step;
}

static inline size_t bit_reverse(size_t index, size_t log2n) {
  size_t reversed = 0;
  for (size_t i = 0; i < log2n; i++) {
    reversed <<= 1;
    reversed |= (index & 1);
    index >>= 1;
  }
  return reversed;
}

void iter_fft(float *in, float *coeffs, Compf *out, size_t size) {
  for (size_t i = 0; i < size; i++) {
    int rev_index = bit_reverse(i, log2(size));
    out[i] = c_from_real(window(in[rev_index], coeffs[rev_index]));
  }

  const Compf iota = c_from_imag(1.0f);
  for (int stage = 1; stage <= log2(size); ++stage) {
    size_t sub_arr_size = 1 << stage; // 2^stage
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

void calculate_window(float *hambuf) {
  const float PI = 3.14159265359f;
  for (int i = 0; i < BUFFER_SIZE; ++i) {
    float t = (float)i / BUFFER_SIZE - 1;
    hambuf[i] = 0.54 - 0.46 * cosf(2 * PI * t);
  }
}

void compf_to_float(float *half, Compf *fft_output) {
  const size_t half_size = BUFFER_SIZE / 2;
  for (size_t i = 0; i < half_size; i++) {
    const Compf *const c = &fft_output[i];
    half[i] = sqrtf(c->real * c->real + c->imag * c->imag);
  }
}

static void gen_bins(float *bins, const int size) {
  const float MAX_FREQ = 20000.0f;
  const float MIN_FREQ = 20.0f;
  const float RATIO = MAX_FREQ / MIN_FREQ;
  for (int i = 0; i < size; i++) {
    float t = (float)i / size;
    // BASE * POSITION
    // 20 * 1000^0.95 = 14158..
    // 20 * 1000^0.98 = 17419..
    // 20 * 1000^1 = 20000
    float k = MIN_FREQ * powf(RATIO, t);
    bins[i] = k;
  }
}

static void bin_slice(const float freq, const float s, float *sums,
                      float *max) {
  float bins[DIVISOR + 1];
  gen_bins(bins, DIVISOR + 1);
  for (int j = 0; j < DIVISOR; j++) {
    if (freq >= bins[j] && freq < bins[j + 1]) {
      sums[j] += s;
      if (sums[j] > *max) {
        *max = sums[j];
      }
      return;
    }
  }
}

void section_bins(const int sr, float *half, float *sums) {
  const int half_size = BUFFER_SIZE / 2;
  float max = 0.0f;
  for (int i = 0; i < half_size; i++) {
    const float freq = i * (float)sr / BUFFER_SIZE;
    bin_slice(freq, half[i], sums, &max);
  }

  for (int l = 0; l < DIVISOR; l++) {
    sums[l] /= max;
  }
}
