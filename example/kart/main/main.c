#define YARI_MAIN
#define YARI_NO_PREFIX
#include <yari.h>
#include <stdio.h>
#include "assets.h"
#include "fonts.h"
#include "level.h"

#define RAY_RES 1
#ifdef ESP32
#define TARGET_FPS 30
#define SCREEN_W 240
#define SCREEN_H 136
#else
#define TARGET_FPS 60
#define SCREEN_W 800
#define SCREEN_H 600
#endif

#define PLAYER_ROTATION_SPEED 0.25f
#define PLAYER_MAX_ROTATION_SPEED 3.0f
#define PLAYER_MAX_SPEED 9.0f
#define PLAYER_ACCELERATION 12.0f
#define PLAYER_BRAKE_DECELERATION 20.0f
#define PLAYER_CRASH_DECELERATION 50.0f


int joystick_id;

// Menu ctx (persists across frames)
static int g_show_fps  = 1;

typedef struct {
    float speed;
    int stearing; // -1 left, 0 straight, 1 right
    bool is_colliding;
} PlayerCtx;
PlayerCtx playerCtx;
int game_state = 0;

#define is_slowing(ctx) \
    ((ctx)->map.floor[(int)(ctx)->camera.pos.x + ((int)(ctx)->camera.pos.y * (ctx)->map.cols)] != tx_wal_000 && \
     (ctx)->map.floor[(int)(ctx)->camera.pos.x + ((int)(ctx)->camera.pos.y * (ctx)->map.cols)] != tx_wal_002 && \
     (ctx)->map.floor[(int)(ctx)->camera.pos.x + ((int)(ctx)->camera.pos.y * (ctx)->map.cols)] != tx_wal_005 && \
     (ctx)->map.floor[(int)(ctx)->camera.pos.x + ((int)(ctx)->camera.pos.y * (ctx)->map.cols)] != 0)


void move_player(Context *ctx) {
    Camera *p = &ctx->camera;
    float joy_x = esp_joystick_get_axis(joystick_id, YR_X_AXIS);
    playerCtx.stearing = 0;

    #ifdef ESP32
    float turn_factor = playerCtx.speed * PLAYER_ROTATION_SPEED * fabs(joy_x);
    #else
    float turn_factor = playerCtx.speed * PLAYER_ROTATION_SPEED;
    #endif
    if (is_key_down(YR_KEY_A) || joy_x < -0.1f) {
        if (turn_factor > PLAYER_MAX_ROTATION_SPEED) turn_factor = PLAYER_MAX_ROTATION_SPEED;
        else if (turn_factor < -PLAYER_MAX_ROTATION_SPEED) turn_factor = -PLAYER_MAX_ROTATION_SPEED;
        p->dir = rotate(p->dir, YR_COUNTERCLOCKWISE, turn_factor);
        playerCtx.stearing = -1;
    }
    if (is_key_down(YR_KEY_D) || joy_x > 0.1f) {
        if (turn_factor > PLAYER_MAX_ROTATION_SPEED) turn_factor = PLAYER_MAX_ROTATION_SPEED;
        else if (turn_factor < -PLAYER_MAX_ROTATION_SPEED) turn_factor = -PLAYER_MAX_ROTATION_SPEED;
        p->dir = rotate(p->dir, YR_CLOCKWISE, turn_factor);
        playerCtx.stearing = 1;
    }

    Vector2 target = p->pos;
    bool is_accelerating = false;
    bool is_braking = false;
    if (is_key_down(YR_KEY_W) || is_key_down(YR_KEY_SPACE)) {
        is_accelerating = true;
        playerCtx.speed += get_frame_time() * PLAYER_ACCELERATION;
        if (playerCtx.speed > PLAYER_MAX_SPEED) {
            playerCtx.speed = PLAYER_MAX_SPEED;
        }
    }
    if (is_key_down(YR_KEY_S) || is_key_down(YR_KEY_X)) {
        is_braking = true;
        playerCtx.speed -= get_frame_time() * PLAYER_BRAKE_DECELERATION;
        if (playerCtx.speed < -PLAYER_MAX_SPEED / 2.0f) {
            playerCtx.speed = -PLAYER_MAX_SPEED / 2.0f;
        }
    }
    if (!is_accelerating && !is_braking) {
        // natural deceleration
        if (playerCtx.speed > 0) {
            playerCtx.speed -= get_frame_time() * PLAYER_ACCELERATION;
            if (playerCtx.speed < 0) playerCtx.speed = 0;
        } else if (playerCtx.speed < 0) {
            playerCtx.speed += get_frame_time() * PLAYER_ACCELERATION;
            if (playerCtx.speed > 0) playerCtx.speed = 0;
        }
    }
    target = move(target, p->dir, YR_FORWARD, playerCtx.speed);
    
    CollisionInfo hit;
    p->pos = slide_collision(ctx, p->pos, target, &hit, YR_PLAYER_COLLISION_THRESHOLD, YR_CMSK_PLAYER);
    if (hit.type == YR_COLLISION_WALL) {
        playerCtx.is_colliding = true;
        playerCtx.speed -= get_frame_time() * PLAYER_CRASH_DECELERATION;
        if (playerCtx.speed < 0) playerCtx.speed = 0;
    } else {
        playerCtx.is_colliding = false;
    }

    if (is_slowing(ctx) && fabs(playerCtx.speed) > 2.0f) {
        playerCtx.speed -= get_frame_time() * PLAYER_BRAKE_DECELERATION;
        if (playerCtx.speed < 0) playerCtx.speed = 0;
    }
}

void draw_hud(Context *ctx) {
    (void)ctx;

    char hp_text[32];
    sprintf(hp_text, "Speed: %.1f", playerCtx.speed);
    draw_text_ex(hp_text, fonts[YR_FONT_SM], YR_GREEN, .align=YR_LAY_TL, .x=10, .y=16);

    int size = ctx->screen_height / 4;
    if(playerCtx.is_colliding) {
        draw_texture_ex(assets_map[tx_auto04], 256, 64, .align=YR_LAY_CB, .height=size);
    } else {
        switch(playerCtx.stearing) {
            case -1: draw_texture_ex(assets_map[tx_auto03], 256, 64, .align=YR_LAY_CB, .height=size); break;
            case  0: draw_texture_ex(assets_map[tx_auto01], 256, 64, .align=YR_LAY_CB, .height=size); break;
            case  1: draw_texture_ex(assets_map[tx_auto02], 256, 64, .align=YR_LAY_CB, .height=size); break;
        }
    }
}

void print_fps() {
    float fps = get_fps();
    char fps_text[32];
    sprintf(fps_text, "FPS: %.1f", fps);
    draw_text_ex(fps_text, fonts[YR_FONT_SM], YR_WHITE, .align=YR_LAY_TR, .x=10, .y=16);
}


// ── Menu ─────────────────────────────────────────────────────────────────────

static void draw_menu(Context *ctx) {
    (void)ctx;
    draw_text_ex("Press SPACE to start", fonts[YR_FONT_MD], YR_WHITE, .align=YR_LAY_CENTER);
    if (is_key_pressed(YR_KEY_SPACE)) {
        game_state = 1;
    }
}

// Main game functions
void yr_init_game(Context *ctx) {
  ctx->screen_width = SCREEN_W;
  ctx->screen_height = SCREEN_H;
  ctx->ray_res = RAY_RES;
  ctx->target_fps = TARGET_FPS;
  load_level(ctx);

  joystick_id = esp_joystick_init(32, 36);
  esp_key_init(25, YR_KEY_Q);
  esp_key_init(2, YR_KEY_E);
  esp_key_init(15, YR_KEY_X);
  esp_key_init(26, YR_KEY_SPACE);
}

void yr_update_game(Context *ctx) {
    if (game_state == 0) {
        draw_menu(ctx);
    } else {
        draw_game(ctx);
        draw_hud(ctx);
        move_player(ctx);
        if (g_show_fps) print_fps();
    }
}
