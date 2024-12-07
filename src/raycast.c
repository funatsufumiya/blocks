#include <stdbool.h>
#include "block.h"
#include "config.h"
#include "helpers.h"
#include "raycast.h"
#include "world.h"

bool raycast(
    float* x,
    float* y,
    float* z,
    const float dx,
    const float dy,
    const float dz,
    const bool previous)
{
    assert(x);
    assert(y);
    assert(z);
    const float length = 10.0f;
    const float step = 0.02f;
    for (float i = 0.0f; i < length; i += step)
    {
        float a = *x + dx * i;
        float b = *y + dy * i;
        float c = *z + dz * i;
        if (a <= 0.0f)
        {
            a -= 1.0f;
        }
        if (c <= 0.0f)
        {
            c -= 1.0f;
        }
        if (block_solid(world_get_block(a, b, c)))
        {
            if (previous)
            {
                a -= dx * step;
                b -= dy * step;
                c -= dz * step;
            }
            if (a < 0.0f && a > -1.0f && dx > 0.0f)
            {
                a = -1.0f;
            }
            if (c < 0.0f && c > -1.0f && dz > 0.0f)
            {
                c = -1.0f;
            }
            *x = a;
            *y = b;
            *z = c;
            return true;
        }
    }
    return false;
}