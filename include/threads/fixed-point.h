/* Project 1 */
#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H
#include <stdint.h>

static const int f = 1 << 14;

static int itofp (int n) {
    return n * f;
}

static int fptoi (int x) { //rounding toward zero
    return x / f;
}

static int fptoird (int x) { //rounding to nearest
    return x >= 0 ? (x + f / 2) / f : (x - f / 2) / f;
}

static int fpadd (int x, int y) {
    return x + y;
}

static int fpsub (int x, int y) {
    return x - y;
}

static int fpaddn (int x, int n) {
    return x + n * f;
}

static int fpsubn (int x, int n) {
    return x - n * f;
}

static int fpmult (int x, int y) {
    return ((int64_t) x) * y / f;
}

static int fpmultn (int x, int n) {
    return x * n;
}

static int fpdiv (int x, int y) {
    return ((int64_t) x) * f / y;
}

static int fpdivn (int x, int n) {
    return x / n;
}

#endif /* include/threads/fixed-point.h */
/* Project 1*/