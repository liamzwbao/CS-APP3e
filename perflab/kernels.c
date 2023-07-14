/********************************************************
 * Kernels to be optimized for the CS:APP Performance Lab
 ********************************************************/

#include <stdio.h>
#include <stdlib.h>
#include "defs.h"

/* 
 * Please fill in the following team struct 
 */
team_t team = {
    "Alpaca",              /* Team name */

    "Liam Bao",     /* First member full name */
    "bao.zhiw@northeastern.edu",  /* First member email address */

    "",                   /* Second member full name (leave blank if none) */
    ""                    /* Second member email addr (leave blank if none) */
};

/***************
 * ROTATE KERNEL
 ***************/

/******************************************************
 * Your different versions of the rotate kernel go here
 ******************************************************/

/* 
 * naive_rotate - The naive baseline version of rotate 
 */
char naive_rotate_descr[] = "naive_rotate: Naive baseline implementation";
void naive_rotate(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(dim-1-j, i, dim)] = src[RIDX(i, j, dim)];
}

/* 
 * rotate - Your current working version of rotate
 * IMPORTANT: This is the version you will be graded on
 */
char rotate_descr[] = "rotate: Current working version";
void rotate(int dim, pixel *src, pixel *dst) 
{
    int i, j, k, r, c, d, s;
    int w = 16;
    for (i = 0; i < dim; i += w) {
        for (j = 0; j < dim; j += w) {
            r = dim - 1 - j;
            c = j;
            for (k = i; k < i + w; k++) {
                d = RIDX(r, k, dim), s = RIDX(k, c, dim);
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
                d -= dim, s++;
                dst[d] = src[s];
            }
        }
    }
}

/*********************************************************************
 * register_rotate_functions - Register all of your different versions
 *     of the rotate kernel with the driver by calling the
 *     add_rotate_function() for each test function. When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_rotate_functions() 
{
    add_rotate_function(&naive_rotate, naive_rotate_descr);   
    add_rotate_function(&rotate, rotate_descr);   
    /* ... Register additional test functions here */
}


/***************
 * SMOOTH KERNEL
 **************/

/***************************************************************
 * Various typedefs and helper functions for the smooth function
 * You may modify these any way you like.
 **************************************************************/

/* A struct used to compute averaged pixel value */
typedef struct {
    int red;
    int green;
    int blue;
    int num;
} pixel_sum;

/* Compute min and max of two integers, respectively */
static int min(int a, int b) { return (a < b ? a : b); }
static int max(int a, int b) { return (a > b ? a : b); }

/* 
 * initialize_pixel_sum - Initializes all fields of sum to 0 
 */
static void initialize_pixel_sum(pixel_sum *sum) 
{
    sum->red = sum->green = sum->blue = 0;
    sum->num = 0;
    return;
}

/* 
 * accumulate_sum - Accumulates field values of p in corresponding 
 * fields of sum 
 */
static void accumulate_sum(pixel_sum *sum, pixel p) 
{
    sum->red += (int) p.red;
    sum->green += (int) p.green;
    sum->blue += (int) p.blue;
    sum->num++;
    return;
}

/* 
 * assign_sum_to_pixel - Computes averaged pixel value in current_pixel 
 */
static void assign_sum_to_pixel(pixel *current_pixel, pixel_sum sum) 
{
    current_pixel->red = (unsigned short) (sum.red/sum.num);
    current_pixel->green = (unsigned short) (sum.green/sum.num);
    current_pixel->blue = (unsigned short) (sum.blue/sum.num);
    return;
}

/* 
 * avg - Returns averaged pixel value at (i,j) 
 */
static pixel avg(int dim, int i, int j, pixel *src) 
{
    int ii, jj;
    pixel_sum sum;
    pixel current_pixel;

    initialize_pixel_sum(&sum);
    for(ii = max(i-1, 0); ii <= min(i+1, dim-1); ii++) 
	for(jj = max(j-1, 0); jj <= min(j+1, dim-1); jj++) 
	    accumulate_sum(&sum, src[RIDX(ii, jj, dim)]);

    assign_sum_to_pixel(&current_pixel, sum);
    return current_pixel;
}

void smooth_corner(pixel *src, pixel *dst, int i1, int i2, int i3, int i4)
{
    dst[i1].red = (src[i1].red + src[i2].red + src[i3].red + src[i4].red) >> 2;
    dst[i1].green = (src[i1].green + src[i2].green + src[i3].green + src[i4].green) >> 2;
    dst[i1].blue = (src[i1].blue + src[i2].blue + src[i3].blue + src[i4].blue) >> 2;
}

void smooth_edge(pixel *src, pixel *dst, int i1, int i2, int i3, int i4, int i5, int i6, int end, int step)
{
    while (i1 < end) {
        dst[i1].red = (src[i1].red + src[i2].red + src[i3].red +
                       src[i4].red + src[i5].red + src[i6].red) / 6;
        dst[i1].green = (src[i1].green + src[i2].green + src[i3].green +
                         src[i4].green + src[i5].green + src[i6].green) / 6;
        dst[i1].blue = (src[i1].blue + src[i2].blue + src[i3].blue +
                        src[i4].blue + src[i5].blue + src[i6].blue) / 6;
        i1 += step, i2 += step, i3 += step, i4 += step, i5 += step, i6 += step;
    }
}

void smooth_interior(int dim, pixel *src, pixel *dst)
{
    int i, j, cur = dim;
    for (i = 1; i < dim - 1; i++) {
        for (j = 1; j < dim - 1; j++) {
            cur++;
            dst[cur].red = (src[cur].red + src[cur - 1].red + src[cur + 1].red +
                            src[cur - dim].red + src[cur - dim - 1].red + src[cur - dim + 1].red +
                            src[cur + dim].red + src[cur + dim - 1].red + src[cur + dim + 1].red) / 9;
            dst[cur].green = (src[cur].green + src[cur - 1].green + src[cur + 1].green +
                              src[cur - dim].green + src[cur - dim - 1].green + src[cur - dim + 1].green +
                              src[cur + dim].green + src[cur + dim - 1].green + src[cur + dim + 1].green) / 9;
            dst[cur].blue = (src[cur].blue + src[cur - 1].blue + src[cur + 1].blue +
                             src[cur - dim].blue + src[cur - dim - 1].blue + src[cur - dim + 1].blue +
                             src[cur + dim].blue + src[cur + dim - 1].blue + src[cur + dim + 1].blue) / 9;
        }
        cur += 2;
    }
}

/******************************************************
 * Your different versions of the smooth kernel go here
 ******************************************************/

/*
 * naive_smooth - The naive baseline version of smooth 
 */
char naive_smooth_descr[] = "naive_smooth: Naive baseline implementation";
void naive_smooth(int dim, pixel *src, pixel *dst) 
{
    int i, j;

    for (i = 0; i < dim; i++)
	for (j = 0; j < dim; j++)
	    dst[RIDX(i, j, dim)] = avg(dim, i, j, src);
}

/*
 * smooth - Your current working version of smooth. 
 * IMPORTANT: This is the version you will be graded on
 */
char smooth_descr[] = "smooth: Current working version";
void smooth(int dim, pixel *src, pixel *dst) 
{
    // Smooth four corners
    int i1, i2, i3, i4;
    // Top-left corner
    i1 = 0, i2 = i1 + 1, i3 = i1 + dim, i4 = i3 + 1;
    smooth_corner(src, dst, i1, i2, i3, i4);
    // Top-right corner
    i1 = dim - 1, i2 = i1 - 1, i3 = i1 + dim, i4 = i3 - 1;
    smooth_corner(src, dst, i1, i2, i3, i4);
    // Bottom-left corner
    i1 *= dim, i2 = i1 + 1, i3 = i1 - dim, i4 = i3 + 1;
    smooth_corner(src, dst, i1, i2, i3, i4);
    // Bottom-right corner
    i1 += dim - 1, i2 = i1 - 1, i3 = i1 - dim, i4 = i3 - 1;
    smooth_corner(src, dst, i1, i2, i3, i4);

    // Smooth four edges
    int i5, i6, end;
    // Top edge
    i1 = 1, i2 = i1 - 1, i3 = i1 + 1, i4 = i1 + dim, i5 = i4 - 1, i6 = i4 + 1;
    end = i1 + dim - 2;
    smooth_edge(src, dst, i1, i2, i3, i4, i5, i6, end, 1);
    // Bottom edge
    i1 = (dim - 1) * dim + 1, i2 = i1 - 1, i3 = i1 + 1, i4 = i1 - dim, i5 = i4 - 1, i6 = i4 + 1;
    end = i1 + dim - 2;
    smooth_edge(src, dst, i1, i2, i3, i4, i5, i6, end, 1);
    // Left edge
    i1 = dim, i2 = i1 - dim, i3 = i1 + dim, i4 = i1 + 1, i5 = i4 - dim, i6 = i4 + dim;
    end = i1 + (dim - 2) * dim;
    smooth_edge(src, dst, i1, i2, i3, i4, i5, i6, end, dim);
    // Right edge
    i1 = 2 * dim - 1, i2 = i1 - dim, i3 = i1 + dim, i4 = i1 - 1, i5 = i4 - dim, i6 = i4 + dim;
    end = i1 + (dim - 2) * dim;
    smooth_edge(src, dst, i1, i2, i3, i4, i5, i6, end, dim);

    // Interior pixels
    smooth_interior(dim, src, dst);
}


/********************************************************************* 
 * register_smooth_functions - Register all of your different versions
 *     of the smooth kernel with the driver by calling the
 *     add_smooth_function() for each test function.  When you run the
 *     driver program, it will test and report the performance of each
 *     registered test function.  
 *********************************************************************/

void register_smooth_functions() {
    add_smooth_function(&naive_smooth, naive_smooth_descr);
    add_smooth_function(&smooth, smooth_descr);
    /* ... Register additional test functions here */
}

