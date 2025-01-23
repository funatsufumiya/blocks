#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
// #include <threads.h>
#include "tinycthread.h"
#include "helpers.h"

const int directions[][3] =
{
    [DIRECTION_N] = { 0, 0, 1 },
    [DIRECTION_S] = { 0, 0,-1 },
    [DIRECTION_E] = { 1, 0, 0 },
    [DIRECTION_W] = {-1, 0, 0 },
    [DIRECTION_U] = { 0, 1, 0 },
    [DIRECTION_D] = { 0,-1, 0 },
};

static int cx;
static int cz;

static int squared(
    const int x,
    const int z)
{
    const int dx = x - cx;
    const int dz = z - cz;
    return dx * dx + dz * dz;
}

static int compare(
    const void* a,
    const void* b)
{
    const int* l = a;
    const int* r = b;
    const int c = squared(l[0], l[1]);
    const int d = squared(r[0], r[1]);
    if (c < d)
    {
        return -1;
    }
    else if (c > d)
    {
        return 1;
    }
    else
    {
        return 0;
    }
}

void sort_2d(
    const int x,
    const int z,
    void* data,
    const int size)
{
    assert(data);
    assert(size);
    cx = x;
    cz = z;
    qsort(data, size, 8, compare);
}