#ifndef YR_YARI_H
#define YR_YARI_H

#define RAYMATH_STATIC_INLINE
#include <stddef.h>
#include "raymath.h"
#include "renderer.h"
#include "inputs.h"
#include "colors.h"
#include "yari_utils.h"
#include "da.h"
#include "ht.h"




#define YR_CMSK_NONE    0
#define YR_CMSK_WALL    1
#define YR_CMSK_ALL    -1

typedef struct YrContext YrContext;
typedef struct YrEntity YrEntity;

typedef void (*YrEntityUpdateFunc)(YrContext *ctx, YrEntity *self, size_t index);
typedef void (*YrEntityInitFunc)(YrEntity *self, void *data);
typedef void (*YrEntityCleanupFunc)(YrEntity *self);

struct YrEntity {
    Vector2 pos;
    int texture_id;
    int kind;
    float dist;
    float vdiv;
    float hdiv;
    float vmove;
    bool disabled;
    void *entity_data;
    uint32_t collision_mask;
    float collision_threshold;
    YrAnimationStack animation;
    YrEntityInitFunc init;
    YrEntityCleanupFunc cleanup;
    YrEntityUpdateFunc update;
};

typedef struct {
    Vector2 pos;
    Vector2 dir;
    float horizon;
    float angle;
} YrCamera;

yr_hm_declare(YrEntityMap, size_t, YrEntity);

struct YrContext {
    YrCamera camera;
    int screen_width;
    int screen_height;
    char *game_title;
    unsigned int target_fps;
    uint8_t *map;
    uint8_t *map_floor;
    uint8_t *map_ceil;
    uint8_t map_cols;
    uint8_t map_rows;
    YrEntityMap entities;
    unsigned int ray_res;
    float *zbuffer;
    const yr_pixel_t **assets_map;
    size_t floor_texture;
    size_t ceil_texture;
    size_t next_entity_id;
    void* game_data; // game-defined state
};

extern YrContext yr_context;


void yr_raycast_walls(YrContext *ctx, Vector2 dir, int slice_x);

void yr_draw_walls(YrContext *ctx);

void yr_draw_background(YrContext *ctx);

void yr_draw_entities(YrContext *ctx);

void yr_draw_game();

size_t yr_create_entity_ex(YrContext *ctx, YrEntity e, void *data);
#define yr_create_entity(state, e) yr_create_entity_ex(state, e, NULL)

void yr_remove_entity(YrContext *ctx, size_t id);
size_t yr_get_entity_id(YrEntity *e);

void yr__init_game();

void yr_init_game(YrContext *ctx);

void yr__update_game();

void yr_update_game(YrContext *ctx);

void yr__free_game();

void yr__draw_walls_range(YrContext *ctx, int x_start, int x_end);
void yr__draw_background_range(YrContext *ctx, int x_start, int x_end);
void yr__draw_sprites_range( YrContext *ctx, YrEntity **entities, size_t active_entities_count, int x_start, int x_end);
size_t yr__entities_prep(YrContext *ctx, YrEntity ***out_entities);
void yr__update_entities(YrContext *ctx);
#ifdef ESP32_MULTITHREAD
void yr__draw_game_multithread();
#endif

#include "physics.h"

#ifdef YARI_NO_PREFIX
#define Camera YrCamera
#define Entity YrEntity
#define Entities YrEntities
#define Context YrContext
#define raycast_walls yr_raycast_walls
#define draw_walls yr_draw_walls
#define draw_background yr_draw_background
#define draw_entities yr_draw_entities
#define draw_game yr_draw_game
#define create_entity yr_create_entity
#define create_entity_ex yr_create_entity_ex
#define remove_entity yr_remove_entity
#define get_entity_id yr_get_entity_id
#endif

#endif // YR_YARI_H

#ifdef YARI_MAIN
#undef YARI_MAIN

#ifdef PLATFORM_WEB
#include <emscripten/emscripten.h>
#endif

#ifdef ESP32
int app_main()
#else
int main()
#endif
{
    yr__init_game();
#ifdef PLATFORM_WEB
    emscripten_set_main_loop(yr__update_game, 0, 1);
#else
    while (!yr_game_should_close()) {
        yr__update_game();
    }
#endif
    yr__free_game();
    return 0;
}

#endif // YARI_MAIN
