#define YARI_MAIN
#define YARI_NO_PREFIX
#include <yari.h>

#define PLAYER_ROTATION_SPEED 2
#define PLAYER_SPEED 4.5
#define PLAYER_COLLISION_THRESHOLD 0.15f

#define COLS 20
#define ROWS 20
// 0 null, 1-127 texture_id, 128-255 color_id
static uint8_t map[ROWS][COLS] = {0};


void init_map() {
    // border
    for (int i = 0; i < ROWS; i++) {
        map[i][0] = YR_WALL_GREEN;
        map[i][COLS - 1] = YR_WALL_GREEN;
    }
    for (int j = 0; j < COLS; j++) {
        map[0][j] = YR_WALL_GREEN;
        map[ROWS - 1][j] = YR_WALL_GREEN;
    }

    // inner blocks
    map[7][7] = YR_WALL_RED;
    map[8][8] = YR_WALL_BLUE;
    map[9][9] = YR_WALL_YELLOW;
}

void move_player(Context *ctx) {
    Camera *p = &ctx->camera;
    if (is_key_down(YR_KEY_A)) {
        p->dir = rotate(p->dir, YR_COUNTERCLOCKWISE, PLAYER_ROTATION_SPEED);
    }
    if (is_key_down(YR_KEY_D)) {
        p->dir = rotate(p->dir, YR_CLOCKWISE, PLAYER_ROTATION_SPEED);
    }

    Vector2 target = p->pos;
    if (is_key_down(YR_KEY_W)) target = move(target, p->dir, YR_FORWARD, PLAYER_SPEED);
    if (is_key_down(YR_KEY_S)) target = move(target, p->dir, YR_BACK, PLAYER_SPEED);
    if (is_key_down(YR_KEY_Q)) target = move(target, p->dir, YR_LEFT, PLAYER_SPEED);
    if (is_key_down(YR_KEY_E)) target = move(target, p->dir, YR_RIGHT, PLAYER_SPEED);

    p->pos = slide_collision(ctx, p->pos, target, NULL, PLAYER_COLLISION_THRESHOLD, YR_CMSK_ALL);
}


// Main game functions
void yr_init_game(Context *ctx) {
  init_map();
  ctx->map = (uint8_t *)map;
  ctx->map_cols = COLS;
  ctx->map_rows = ROWS;
  ctx->camera = (Camera){.pos = {14.5, 5.5}, .dir = {-0.8, 0.5}};
}

void yr_update_game(Context *ctx) {
    draw_game();
    move_player(ctx);
}
