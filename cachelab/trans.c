/* 
 * trans.c - Matrix transpose B = A^T
 *
 * Each transpose function must have a prototype of the form:
 * void trans(int M, int N, int A[N][M], int B[M][N]);
 *
 * A transpose function is evaluated by counting the number of misses
 * on a 1KB direct mapped cache with a block size of 32 bytes.
 */
#include <stdio.h>
#include "cachelab.h"

int is_transpose(int M, int N, int A[N][M], int B[M][N]);

void trans(int M, int N, int A[N][M], int B[M][N]);

void trans_32x32(int M, int N, int A[N][M], int B[M][N]);

void trans_64x64(int M, int N, int A[N][M], int B[M][N]);

void trans_61x67(int M, int N, int A[N][M], int B[M][N]);

/* 
 * transpose_submit - This is the solution transpose function that you
 *     will be graded on for Part B of the assignment. Do not change
 *     the description string "Transpose submission", as the driver
 *     searches for that string to identify the transpose function to
 *     be graded. 
 */
char transpose_submit_desc[] = "Transpose submission";
void transpose_submit(int M, int N, int A[N][M], int B[M][N])
{
    if (M == 32 && N == 32) trans_32x32(M, N, A, B);
    else if (M == 64 && N == 64) trans_64x64(M, N, A, B);
    else if (M == 61 && N == 67) trans_61x67(M, N, A, B);
    else trans(M, N, A, B);
}

/* 
 * You can define additional transpose functions below. We've defined
 * a simple one below to help you get started. 
 */

/* 
 * trans - A simple baseline transpose function, not optimized for the cache.
 */
char trans_desc[] = "Simple row-wise scan transpose";
void trans(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, tmp;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; j++) {
            tmp = A[i][j];
            B[j][i] = tmp;
        }
    }
}

/*
 * trans_32x32 - A transpose function optimized for matrix size of 32 x 32.
 */
char trans_desc_32x32[] = "Transpose for 32 x 32";
void trans_32x32(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;

    for (i = 0; i < N; i += 8) {
        for (j = 0; j < M; j += 8) {
            for (k = i; k < i + 8; k++) {
                tmp0 = A[k][j];
                tmp1 = A[k][j + 1];
                tmp2 = A[k][j + 2];
                tmp3 = A[k][j + 3];
                tmp4 = A[k][j + 4];
                tmp5 = A[k][j + 5];
                tmp6 = A[k][j + 6];
                tmp7 = A[k][j + 7];

                B[j][k] = tmp0;
                B[j + 1][k] = tmp1;
                B[j + 2][k] = tmp2;
                B[j + 3][k] = tmp3;
                B[j + 4][k] = tmp4;
                B[j + 5][k] = tmp5;
                B[j + 6][k] = tmp6;
                B[j + 7][k] = tmp7;
            }
        }
    }
}

/*
 * trans_64x64 - A transpose function optimized for matrix size of 64 x 64.
 */
char trans_desc_64x64[] = "Transpose for 64 x 64";
void trans_64x64(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k, l, tmp0, tmp1, tmp2, tmp3, tmp4, tmp5, tmp6, tmp7;

    for (i = 0; i < N; i += 8) {
        for (j = 0; j < M; j += 8) {
            for (k = i; k < i + 4; k++) {
                tmp0 = A[k][j];
                tmp1 = A[k][j + 1];
                tmp2 = A[k][j + 2];
                tmp3 = A[k][j + 3];
                tmp4 = A[k][j + 4];
                tmp5 = A[k][j + 5];
                tmp6 = A[k][j + 6];
                tmp7 = A[k][j + 7];

                B[j][k] = tmp0;
                B[j + 1][k] = tmp1;
                B[j + 2][k] = tmp2;
                B[j + 3][k] = tmp3;
                B[j + 3][k + 4] = tmp4;
                B[j + 2][k + 4] = tmp5;
                B[j + 1][k + 4] = tmp6;
                B[j][k + 4] = tmp7;
            }

            for (l = 0; l < 4; l++) {
                tmp0 = A[i + 4][j + 3 - l];
                tmp1 = A[i + 5][j + 3 - l];
                tmp2 = A[i + 6][j + 3 - l];
                tmp3 = A[i + 7][j + 3 - l];
                tmp4 = A[i + 4][j + 4 + l];
                tmp5 = A[i + 5][j + 4 + l];
                tmp6 = A[i + 6][j + 4 + l];
                tmp7 = A[i + 7][j + 4 + l];

                B[j + 4 + l][i] = B[j + 3 - l][i + 4];
                B[j + 4 + l][i + 1] = B[j + 3 - l][i + 5];
                B[j + 4 + l][i + 2] = B[j + 3 - l][i + 6];
                B[j + 4 + l][i + 3] = B[j + 3 - l][i + 7];

                B[j + 3 - l][i + 4] = tmp0;
                B[j + 3 - l][i + 5] = tmp1;
                B[j + 3 - l][i + 6] = tmp2;
                B[j + 3 - l][i + 7] = tmp3;
                B[j + 4 + l][i + 4] = tmp4;
                B[j + 4 + l][i + 5] = tmp5;
                B[j + 4 + l][i + 6] = tmp6;
                B[j + 4 + l][i + 7] = tmp7;
            }
        }
    }
}

/*
 * trans_61x67 - A transpose function optimized for matrix size of 61 x 67.
 */
char trans_desc_61x67[] = "Transpose for 61 x 67";
void trans_61x67(int M, int N, int A[N][M], int B[M][N])
{
    int i, j, k, l, step = 16;

    for (i = 0; i < N; i += step) {
        for (j = 0; j < M; j += step) {
            for (k = i; k < i + step && k < N; k++) {
                for (l = j; l < j + step && l < M; l++) {
                    B[l][k] = A[k][l];
                }
            }
        }
    }
}

/*
 * registerFunctions - This function registers your transpose
 *     functions with the driver.  At runtime, the driver will
 *     evaluate each of the registered functions and summarize their
 *     performance. This is a handy way to experiment with different
 *     transpose strategies.
 */
void registerFunctions()
{
    /* Register your solution function */
    registerTransFunction(transpose_submit, transpose_submit_desc);

    /* Register any additional transpose functions */
    registerTransFunction(trans, trans_desc);
}

/* 
 * is_transpose - This helper function checks if B is the transpose of
 *     A. You can check the correctness of your transpose by calling
 *     it before returning from the transpose function.
 */
int is_transpose(int M, int N, int A[N][M], int B[M][N])
{
    int i, j;

    for (i = 0; i < N; i++) {
        for (j = 0; j < M; ++j) {
            if (A[i][j] != B[j][i]) {
                return 0;
            }
        }
    }
    return 1;
}

