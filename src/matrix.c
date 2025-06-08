#include "matrix.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include <SDL2/SDL_timer.h>

Matrix translate_mat(const float x, const float y, const float z) {
  Matrix m = identity();
  m.m12 += x;
  m.m13 += y;
  m.m14 += z;
  return m;
}

Matrix identity(void) {
  Matrix id = {
      1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
  };
  return id;
}

Matrix rotate_maty(const float angle) {
  // Yeah I read the raylib src code what about u bich
  const float c = cosf(angle * DEG2RAD);
  const float s = sinf(angle * DEG2RAD);

  Matrix m = identity();
  m.m0 = c;
  m.m2 = -s;
  m.m8 = s;
  m.m10 = c;

  return m;
}

Matrix rotate_matx(const float angle) {
  // Yeah I read the raylib src code what about u bich
  const float c = cosf(angle * DEG2RAD);
  const float s = sinf(angle * DEG2RAD);

  Matrix m = identity();
  m.m5 = c;
  m.m9 = -s;
  m.m6 = s;
  m.m10 = c;

  return m;
}

Matrix rms_identity(const float str) {
  const float scale = 1.0f * str; // interpolate_str(strength);

  Matrix id = {
      scale, 0.0f, 0.0f,  0.0f, 0.0f, scale, 0.0f, 0.0f,
      0.0f,  0.0f, scale, 0.0f, 0.0f, 0.0f,  0.0f, 1.0f,
  };

  return id;
}

void print_mat(Matrix *mat) {
  printf("%.3f, %.3f, %.3f, %.3f\n", mat->m0, mat->m4, mat->m8, mat->m12);
  printf("%.3f, %.3f, %.3f, %.3f\n", mat->m1, mat->m5, mat->m9, mat->m13);
  printf("%.3f, %.3f, %.3f, %.3f\n", mat->m2, mat->m6, mat->m10, mat->m14);
  printf("%.3f, %.3f, %.3f, %.3f\n", mat->m3, mat->m7, mat->m11, mat->m15);
}

Matrix multiply_mat(Matrix a, Matrix b) {
  Matrix result;

  result.m0 = a.m0 * b.m0 + a.m4 * b.m1 + a.m8 * b.m2 + a.m12 * b.m3;
  result.m1 = a.m1 * b.m0 + a.m5 * b.m1 + a.m9 * b.m2 + a.m13 * b.m3;
  result.m2 = a.m2 * b.m0 + a.m6 * b.m1 + a.m10 * b.m2 + a.m14 * b.m3;
  result.m3 = a.m3 * b.m0 + a.m7 * b.m1 + a.m11 * b.m2 + a.m15 * b.m3;

  result.m4 = a.m0 * b.m4 + a.m4 * b.m5 + a.m8 * b.m6 + a.m12 * b.m7;
  result.m5 = a.m1 * b.m4 + a.m5 * b.m5 + a.m9 * b.m6 + a.m13 * b.m7;
  result.m6 = a.m2 * b.m4 + a.m6 * b.m5 + a.m10 * b.m6 + a.m14 * b.m7;
  result.m7 = a.m3 * b.m4 + a.m7 * b.m5 + a.m11 * b.m6 + a.m15 * b.m7;

  result.m8 = a.m0 * b.m8 + a.m4 * b.m9 + a.m8 * b.m10 + a.m12 * b.m11;
  result.m9 = a.m1 * b.m8 + a.m5 * b.m9 + a.m9 * b.m10 + a.m13 * b.m11;
  result.m10 = a.m2 * b.m8 + a.m6 * b.m9 + a.m10 * b.m10 + a.m14 * b.m11;
  result.m11 = a.m3 * b.m8 + a.m7 * b.m9 + a.m11 * b.m10 + a.m15 * b.m11;

  result.m12 = a.m0 * b.m12 + a.m4 * b.m13 + a.m8 * b.m14 + a.m12 * b.m15;
  result.m13 = a.m1 * b.m12 + a.m5 * b.m13 + a.m9 * b.m14 + a.m13 * b.m15;
  result.m14 = a.m2 * b.m12 + a.m6 * b.m13 + a.m10 * b.m14 + a.m14 * b.m15;
  result.m15 = a.m3 * b.m12 + a.m7 * b.m13 + a.m11 * b.m14 + a.m15 * b.m15;

  return result;
}

Matrix pers_mat(const float deg, const float aspect_ratio, const float near,
                const float far) {
  Matrix mat = identity();
  const float rad = DEG2RAD * deg;
  const float half = tanf(rad / 2.0f);

  const float x = 1.0f / (aspect_ratio * half);
  const float y = 1.0f / half;

  const float z = -(far + near) / (far - near);
  const float w = -(2 * far * near) / (far - near);

  mat.m0 = x;
  mat.m5 = y;
  mat.m10 = z;
  mat.m11 = -1.0f;
  mat.m14 = w;

  return mat;
}
