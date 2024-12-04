#include <SDL3/SDL.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <threads.h>
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

static thread_local int cx;
static thread_local int cy;
static thread_local int cz;
static thread_local bool is_2d;

static int squared(
    const int x,
    const int y,
    const int z)
{
    const int dx = x - cx;
    const int dy = y - cy;
    const int dz = z - cz;
    return dx * dx + dy * dy + dz * dz;
}

static int compare(
    const void* a,
    const void* b)
{
    const int* l = a;
    const int* r = b;
    int c;
    int d;
    if (is_2d)
    {
        c = squared(l[0], 0, l[1]);
        d = squared(r[0], 0, r[1]);
    }
    else
    {
        c = squared(l[0], l[1], l[2]);
        d = squared(r[0], r[1], r[2]);
    }
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
    cy = 0;
    cz = z;
    is_2d = true;
    qsort(data, size, 8, compare);
}

void sort_3d(
    const int x,
    const int y,
    const int z,
    void* data,
    const int size)
{
    assert(data);
    assert(size);
    cx = x;
    cy = y;
    cz = z;
    is_2d = false;
    qsort(data, size, 12, compare);
}