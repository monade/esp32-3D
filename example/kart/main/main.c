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

// Menu state (persists across frames)
static int g_show_fps  = 1;

typedef struct {
    float speed;
    int stearing; // -1 left, 0 straight, 1 right
    bool is_colliding;
} PlayerState;
PlayerState playerState;
int game_state = 0;

#define is_slowing(state) \
    ((state)->map_floor[(int)(state)->camera.pos.x + ((int)(state)->camera.pos.y * (state)->map_cols)] != tx_wal_000 && \
     (state)->map_floor[(int)(state)->camera.pos.x + ((int)(state)->camera.pos.y * (state)->map_cols)] != tx_wal_002 && \
     (state)->map_floor[(int)(state)->camera.pos.x + ((int)(state)->camera.pos.y * (state)->map_cols)] != tx_wal_005 && \
     (state)->map_floor[(int)(state)->camera.pos.x + ((int)(state)->camera.pos.y * (state)->map_cols)] != 0)


void move_player(GameState *state) {
    Camera *p = &state->camera;
    float joy_x = esp_joystick_get_axis(joystick_id, YR_X_AXIS);
    playerState.stearing = 0;

    #ifdef ESP32
    float turn_factor = playerState.speed * PLAYER_ROTATION_SPEED * fabs(joy_x);
    #else
    float turn_factor = playerState.speed * PLAYER_ROTATION_SPEED;
    #endif
    if (is_key_down(YR_KEY_A) || joy_x < -0.1f) {
        if (turn_factor > PLAYER_MAX_ROTATION_SPEED) turn_factor = PLAYER_MAX_ROTATION_SPEED;
        else if (turn_factor < -PLAYER_MAX_ROTATION_SPEED) turn_factor = -PLAYER_MAX_ROTATION_SPEED;
        p->dir = rotate(p->dir, YR_COUNTERCLOCKWISE, turn_factor);
        playerState.stearing = -1;
    }
    if (is_key_down(YR_KEY_D) || joy_x > 0.1f) {
        if (turn_factor > PLAYER_MAX_ROTATION_SPEED) turn_factor = PLAYER_MAX_ROTATION_SPEED;
        else if (turn_factor < -PLAYER_MAX_ROTATION_SPEED) turn_factor = -PLAYER_MAX_ROTATION_SPEED;
        p->dir = rotate(p->dir, YR_CLOCKWISE, turn_factor);
        playerState.stearing = 1;
    }

    Vector2 target = p->pos;
    bool is_accelerating = false;
    bool is_braking = false;
    if (is_key_down(YR_KEY_W) || is_key_down(YR_KEY_SPACE)) {
        is_accelerating = true;
        playerState.speed += get_frame_time() * PLAYER_ACCELERATION;
        if (playerState.speed > PLAYER_MAX_SPEED) {
            playerState.speed = PLAYER_MAX_SPEED;
        }
    }
    if (is_key_down(YR_KEY_S) || is_key_down(YR_KEY_X)) {
        is_braking = true;
        playerState.speed -= get_frame_time() * PLAYER_BRAKE_DECELERATION;
        if (playerState.speed < -PLAYER_MAX_SPEED / 2.0f) {
            playerState.speed = -PLAYER_MAX_SPEED / 2.0f;
        }
    }
    if (!is_accelerating && !is_braking) {
        // natural deceleration
        if (playerState.speed > 0) {
            playerState.speed -= get_frame_time() * PLAYER_ACCELERATION;
            if (playerState.speed < 0) playerState.speed = 0;
        } else if (playerState.speed < 0) {
            playerState.speed += get_frame_time() * PLAYER_ACCELERATION;
            if (playerState.speed > 0) playerState.speed = 0;
        }
    }
    target = move(target, p->dir, YR_FORWARD, playerState.speed);
    
    CollisionInfo hit;
    p->pos = slide_collision(state, p->pos, target, &hit, YR_PLAYER_COLLISION_THRESHOLD, YR_CMSK_PLAYER);
    if (hit.type == YR_COLLISION_WALL) {
        playerState.is_colliding = true;
        playerState.speed -= get_frame_time() * PLAYER_CRASH_DECELERATION;
        if (playerState.speed < 0) playerState.speed = 0;
    } else {
        playerState.is_colliding = false;
    }

    if (is_slowing(state) && fabs(playerState.speed) > 2.0f) {
        playerState.speed -= get_frame_time() * PLAYER_BRAKE_DECELERATION;
        if (playerState.speed < 0) playerState.speed = 0;
    }
}

void draw_hud(GameState *state) {
    (void)state;

    char hp_text[32];
    sprintf(hp_text, "Speed: %.1f", playerState.speed);
    draw_text(hp_text, 10, 15, fonts[YR_FONT_SM], YR_GREEN);

    int size = state->screen_height / 4;
    if(playerState.is_colliding) {
        draw_texture(0, state->screen_height - size, state->screen_width, size, assets_map[tx_auto04], 256, 64, true);
    } else {
        switch(playerState.stearing) {
            case -1: draw_texture(0, state->screen_height - size, state->screen_width, size, assets_map[tx_auto03], 256, 64, true); break;
            case 0: draw_texture(0, state->screen_height - size, state->screen_width, size, assets_map[tx_auto01], 256, 64, true); break;
            case 1: draw_texture(0, state->screen_height - size, state->screen_width, size, assets_map[tx_auto02], 256, 64, true); break;
        }
    }
}

void print_fps() {
    float fps = get_fps();
    char fps_text[32];
    sprintf(fps_text, "FPS: %.1f", fps);
    draw_text(fps_text, SCREEN_W - 100, 15, fonts[YR_FONT_SM], YR_WHITE);
}


// ── Menu ─────────────────────────────────────────────────────────────────────

static void draw_menu(GameState *state) {
    draw_text("Press SPACE to start", state->screen_width/2-140, state->screen_height/2-10, fonts[YR_FONT_MD], YR_WHITE);
    if (is_key_pressed(YR_KEY_SPACE)) {
        game_state = 1;
    }
}

// Main game functions
void yr_init_game(GameState *state) {
  state->screen_width = SCREEN_W;
  state->screen_height = SCREEN_H;
  state->ray_res = RAY_RES;
  state->target_fps = TARGET_FPS;
  load_level(state);

  joystick_id = esp_joystick_init(32, 36);
  esp_key_init(25, YR_KEY_Q);
  esp_key_init(2, YR_KEY_E);
  esp_key_init(15, YR_KEY_X);
  esp_key_init(26, YR_KEY_SPACE);
}

void yr_update_game(GameState *state) {
    if (game_state == 0) {
        draw_menu(state);
    } else {
        draw_game();
        draw_hud(state);
        move_player(state);
        if (g_show_fps) print_fps();
    }
}
