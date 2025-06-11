#ifndef MATRIX_H
#define MATRIX_H

#define PI      3.14159265359
#define DEG2RAD (PI / 180.0)

// Row major matrix - OpenGL needs to translate it when being passed to render
// funcs
typedef struct
{
    float m0, m4, m8, m12;
    float m1, m5, m9, m13;
    float m2, m6, m10, m14;
    float m3, m7, m11, m15;
} Matrix;

Matrix ortho_mat(float left, float right, float bottom,
                 float top, float near, float far);
Matrix scale_mat(float x, float y, float z);
Matrix translate_mat(float x, float y, float z);
Matrix identity(void);
Matrix rms_identity(float str);
void print_mat(Matrix *mat);
Matrix multiply_mat(Matrix a, Matrix b);
Matrix pers_mat(float deg, float aspect_ratio, float near,
                float far);
Matrix rotate_matx(float angle);
Matrix rotate_maty(float angle);

#endif
