#ifndef MATRIX_H
#define MATRIX_H

#define PI 3.14
#define DEG2RAD (PI / 180.0)

// Row major matrix - OpenGL needs to translate it when being passed to render
// funcs
typedef struct {
  float m0, m4, m8, m12;
  float m1, m5, m9, m13;
  float m2, m6, m10, m14;
  float m3, m7, m11, m15;
} Matrix;

Matrix translate_mat(const float x, const float y, const float z);
Matrix identity(void);
void print_mat(Matrix *mat);
Matrix multiply_mat(Matrix x, Matrix y);
Matrix pers_mat(const float deg, const float aspect_ratio, const float near,
                const float far);

#endif
