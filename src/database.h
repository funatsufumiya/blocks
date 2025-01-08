#pragma once

#include <stdbool.h>
#include "block.h"
#include "ds.h"
#include "helpers.h"
#include "noise.h"

bool database_init(
    const char* file);
void database_free();
void database_commit();
void database_set_player(
    const int id,
    const float x,
    const float y,
    const float z,
    const float pitch,
    const float yaw);
bool database_get_player(
    const int id,
    float* x,
    float* y,
    float* z,
    float* pitch,
    float* yaw);
void database_set_block(
    const int a,
    const int c,
    const int x,
    const int y,
    const int z,
    const block_t block);
void database_get_blocks(
    group_t* group,
    const int a,
    const int c);