#include <SDL3/SDL.h>
#include <sqlite3.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <threads.h>
#include "block.h"
#include "database.h"
#include "chunk.h"
#include "helpers.h"

static sqlite3* handle;
static sqlite3_stmt* set_player_stmt;
static sqlite3_stmt* get_player_stmt;
static sqlite3_stmt* set_block_stmt;
static sqlite3_stmt* get_blocks_stmt;
static mtx_t mtx;

bool database_init(
    const char* file)
{
    assert(file);
    if (sqlite3_open(file, &handle))
    {
        SDL_Log("Failed to open %s database: %s", file, sqlite3_errmsg(handle));
        return false;
    }
    const char* players_table =
        "CREATE TABLE IF NOT EXISTS players ("
        "    id INT PRIMARY KEY NOT NULL,"
        "    x REAL NOT NULL,"
        "    y REAL NOT NULL,"
        "    z REAL NOT NULL,"
        "    pitch REAL NOT NULL,"
        "    yaw REAL NOT NULL"
        ");";
    const char* blocks_table =
        "CREATE TABLE IF NOT EXISTS blocks ("
        "    a INTEGER NOT NULL,"
        "    c INTEGER NOT NULL,"
        "    x INTEGER NOT NULL,"
        "    y INTEGER NOT NULL,"
        "    z INTEGER NOT NULL,"
        "    data INTEGER NOT NULL,"
        "    PRIMARY KEY (a, c, x, y, z)"
        ");";
    if (sqlite3_exec(handle, players_table, NULL, NULL, NULL))
    {
        SDL_Log("Failed to create players table: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_exec(handle, blocks_table, NULL, NULL, NULL))
    {
        SDL_Log("Failed to create blocks table: %s", sqlite3_errmsg(handle));
        return false;
    }
    const char* set_player =
        "INSERT OR REPLACE INTO players (id, x, y, z, pitch, yaw) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    const char* get_player =
        "SELECT x, y, z, pitch, yaw FROM players "
        "WHERE id = ?;";
    const char* set_block =
        "INSERT OR REPLACE INTO blocks (a, c, x, y, z, data) "
        "VALUES (?, ?, ?, ?, ?, ?);";
    const char* get_blocks =
        "SELECT x, y, z, data FROM blocks "
        "WHERE a = ? AND c = ?;";
    if (sqlite3_prepare_v2(handle, set_player, -1, &set_player_stmt, NULL))
    {
        SDL_Log("Failed to prepare set player: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_prepare_v2(handle, get_player, -1, &get_player_stmt, NULL))
    {
        SDL_Log("Failed to prepare get player: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_prepare_v2(handle, set_block, -1, &set_block_stmt, NULL))
    {
        SDL_Log("Failed to prepare set block: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (sqlite3_prepare_v2(handle, get_blocks, -1, &get_blocks_stmt, NULL))
    {
        SDL_Log("Failed to prepare get blocks: %s", sqlite3_errmsg(handle));
        return false;
    }
    const char* blocks_index =
        "CREATE INDEX IF NOT EXISTS blocks_index "
        "ON blocks (a, c);";
    if (sqlite3_exec(handle, blocks_index, NULL, NULL, NULL))
    {
        SDL_Log("Failed to create blocks index: %s", sqlite3_errmsg(handle));
        return false;
    }
    if (mtx_init(&mtx, mtx_plain) != thrd_success)
    {
        SDL_Log("Failed to create mutex");
        return false;
    }
    sqlite3_exec(handle, "BEGIN;", NULL, NULL, NULL);
    return true;
}

void database_free()
{
    mtx_destroy(&mtx);
    sqlite3_exec(handle, "COMMIT;", NULL, NULL, NULL);
    sqlite3_finalize(set_player_stmt);
    sqlite3_finalize(get_player_stmt);
    sqlite3_finalize(set_block_stmt);
    sqlite3_finalize(get_blocks_stmt);
    sqlite3_close(handle);
}

void database_commit()
{
    mtx_lock(&mtx);
    sqlite3_exec(handle, "COMMIT; BEGIN;", NULL, NULL, NULL);
    mtx_unlock(&mtx);
}

void database_set_player(
    const int id,
    const float x,
    const float y,
    const float z,
    const float pitch,
    const float yaw)
{
    mtx_lock(&mtx);
    sqlite3_bind_int(set_player_stmt, 1, id);
    sqlite3_bind_double(set_player_stmt, 2, x);
    sqlite3_bind_double(set_player_stmt, 3, y);
    sqlite3_bind_double(set_player_stmt, 4, z);
    sqlite3_bind_double(set_player_stmt, 5, pitch);
    sqlite3_bind_double(set_player_stmt, 6, yaw);
    if (sqlite3_step(set_player_stmt) != SQLITE_DONE)
    {
        SDL_Log("Failed to set player: %s", sqlite3_errmsg(handle));
    }
    sqlite3_reset(set_player_stmt);
    mtx_unlock(&mtx);
}

bool database_get_player(
    const int id,
    float* x,
    float* y,
    float* z,
    float* pitch,
    float* yaw)
{
    assert(x);
    assert(y);
    assert(z);
    assert(pitch);
    assert(yaw);
    mtx_lock(&mtx);
    sqlite3_bind_int(get_player_stmt, 1, id);
    bool player = sqlite3_step(get_player_stmt) == SQLITE_ROW;
    if (player)
    {
        *x = sqlite3_column_double(get_player_stmt, 0);
        *y = sqlite3_column_double(get_player_stmt, 1);
        *z = sqlite3_column_double(get_player_stmt, 2);
        *pitch = sqlite3_column_double(get_player_stmt, 3);
        *yaw = sqlite3_column_double(get_player_stmt, 4);
    }
    sqlite3_reset(get_player_stmt);
    mtx_unlock(&mtx);
    return player;
}

void database_set_block(
    const int a,
    const int c,
    const int x,
    const int y,
    const int z,
    const block_t block)
{
    mtx_lock(&mtx);
    sqlite3_bind_int(set_block_stmt, 1, a);
    sqlite3_bind_int(set_block_stmt, 2, c);
    sqlite3_bind_int(set_block_stmt, 3, x);
    sqlite3_bind_int(set_block_stmt, 4, y);
    sqlite3_bind_int(set_block_stmt, 5, z);
    sqlite3_bind_int(set_block_stmt, 6, block);
    if (sqlite3_step(set_block_stmt) != SQLITE_DONE)
    {
        SDL_Log("Failed to set block: %s", sqlite3_errmsg(handle));
    }
    sqlite3_reset(set_block_stmt);
    mtx_unlock(&mtx);
}

void database_get_blocks(
    chunk_t* chunk,
    const int a,
    const int c)
{
    assert(chunk);
    mtx_lock(&mtx);
    sqlite3_bind_int(get_blocks_stmt, 1, a);
    sqlite3_bind_int(get_blocks_stmt, 2, c);
    while (sqlite3_step(get_blocks_stmt) == SQLITE_ROW)
    {
        const int x = sqlite3_column_int(get_blocks_stmt, 0);
        const int y = sqlite3_column_int(get_blocks_stmt, 1);
        const int z = sqlite3_column_int(get_blocks_stmt, 2);
        const block_t block = sqlite3_column_int(get_blocks_stmt, 3);
        chunk_set_block(chunk, x, y, z, block);
    }
    sqlite3_reset(get_blocks_stmt);
    mtx_unlock(&mtx);
}