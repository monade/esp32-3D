#define YARI_MAIN
#define YARI_NO_PREFIX
#include <yari.h>
#include <stdio.h>
#include "assets.h"
#include "fonts.h"
#include "level1.h"
#include "level2.h"

#ifdef ESP32
#define TARGET_FPS 30
#else
#define TARGET_FPS 60
#endif

#define PLAYER_ROTATION_SPEED 2.0
#define PLAYER_SPEED 4.0
#define PLAYER_RUN_SPEED 5.5
#define PLAYER_COLLISION_THRESHOLD 0.35f
#define PLAYER_HIT_ANIM_SPEED 0.4f
#define PLAYER_BOB_SPEED 14.0f
#define PLAYER_RUN_BOB_SPEED 20.0f
#define PLAYER_BOB_AMOUNT 0.015f
#define PLAYER_RUN_BOB_AMOUNT 0.025f

#define GUN_SCALE 0.45f

#define ANIMATION_SPEED 0.25f

#define MUMMY_SPEED 4.0
#define MUMMY_DAMAGE_COOLDOWN 0.5
#define BOSS_SHOT_COOLDOWN 1.5
#define PROJECTILE_SPEED 8.0f

int joystick_id;

enum {
    GAME_MENU,
    GAME_LEVEL1,
    GAME_MENU2,
    GAME_LEVEL2,
    GAME_OVER,
    GAME_WIN
};

typedef struct {
    int tx_id;
    float shot_cd;
    int damage;
    float range;
    AnimationStack shot_animation;
    Animation fire_anim;
} GunData;

typedef struct {
    int hp;
    bool has_key;
    bool has_killed_boss;
    GunData gun;
    Timer shot_timer;
    Timer taking_damage;
    float bob_phase;
    float bob_horizon;
} PlayerData;

typedef struct {
    int state;
    PlayerData player;
} GameData;

typedef struct {
    int hp;                   
    Animation hit_anim;       
    Animation attack_anim;    
    Timer shot_cd;
    int damage;
} EnemyData;

typedef struct {
    Vector2 dir;
    int damage;
} ProjectileData;

typedef struct {
    Timer duration;
    Timer damage_cd;
    int damage_player;
    int damage_enemy;
} ExplosionData;

GameData game = {0};

Timer boss_spawn_animation = {0};

struct v2i {
    int x;
    int y;
};

enum {
    WEP_HND,
    WEP_GUN,
    WEP_BFG,
};
static const GunData WEAPONS[] = {
    // WEP_HND
    {
        .tx_id     = tx_wep_hnd0,
        .damage    = 10,
        .range     = 1.5,
        .shot_cd   = 0.7,
        .fire_anim = {.frames=(int[]){tx_wep_hnd1}, .frame_count=1, .duration=ANIMATION_SPEED},
    },
    //WEP_GUN
    {
        .tx_id     = tx_wep_gun0,
        .damage    = 35,
        .range     = YR_MAX_RENDER_DIST,
        .shot_cd   = 0.4,
        .fire_anim = {.frames=(int[]){tx_wep_gun1}, .frame_count=1, .duration=ANIMATION_SPEED},
    },
    //WEP_BFG
    {
        .tx_id     = tx_wep_bfg0,
        .damage    = 100,
        .range     = 5.0,
        .shot_cd   = 0.8,
        .fire_anim = {.frames=(int[]){tx_wep_bfg1}, .frame_count=1, .duration=ANIMATION_SPEED},
    },
};

void pickup_key(YrContext *ctx, YrEntity *self, size_t index) {
    if (self->dist < PLAYER_COLLISION_THRESHOLD + self->collision_threshold) {
        game.player.has_key = true;
        remove_entity(ctx, index);
    }
}

void pickup_medikit(YrContext *ctx, YrEntity *self, size_t index) {
    if (self->dist < PLAYER_COLLISION_THRESHOLD + self->collision_threshold) {
        game.player.hp += 50;
        remove_entity(ctx, index);
    }
}

void trigger_end(YrContext *ctx, YrEntity *self, size_t index) {
    (void)ctx;
    (void)index;
    if (self->dist < PLAYER_COLLISION_THRESHOLD + self->collision_threshold) {
        if (game.player.has_key && game.player.has_killed_boss) {
            game.state = GAME_MENU2;
        } else if (!game.player.has_key) {
            draw_text_ex("You need the key to exit!", fonts[YR_FONT_MD], YR_RED, .align=YR_LAY_CENTER);
        } else if (!game.player.has_killed_boss) {
            draw_text_ex("Kill the boss!", fonts[YR_FONT_MD], YR_RED, .align=YR_LAY_CENTER);
        }
    }
}

void damage_player(int damage) {
    game.player.hp -= damage;
    game.player.taking_damage = timer_start(PLAYER_HIT_ANIM_SPEED);
}

void set_gun(int gun_id) {
    game.player.gun.shot_animation.length=0;
    game.player.gun.tx_id = WEAPONS[gun_id].tx_id;
    game.player.gun.shot_cd = WEAPONS[gun_id].shot_cd;
    game.player.gun.damage = WEAPONS[gun_id].damage;
    game.player.gun.range = WEAPONS[gun_id].range;
    game.player.gun.fire_anim = WEAPONS[gun_id].fire_anim;
}


void spawn_mummy(Context *ctx, Vector2 pos) {
    create_entity(ctx, create_mummy_level1_pos(pos, NULL));
}

void spawn_mummy2(Context *ctx, Vector2 pos) {
    create_entity(ctx, create_mummy_2_level1_pos(pos, NULL));
}

void spawn_boss(Context *ctx) {
    create_entity(ctx, create_boss_level1(NULL));
}

void spawn_first_wave(Context *ctx) {
    spawn_mummy(ctx, (Vector2){.x = 11, .y = 25});
    spawn_mummy(ctx, (Vector2){.x = 12, .y = 29});
    spawn_mummy(ctx, (Vector2){.x = 9, .y = 28});
    spawn_mummy(ctx, (Vector2){.x = 8, .y = 35});
    spawn_mummy(ctx, (Vector2){.x = 8, .y = 29});
}

void spawn_second_wave(Context *ctx) {
    spawn_mummy(ctx, (Vector2){.x = 11, .y = 3});
    spawn_mummy(ctx, (Vector2){.x = 12, .y = 2});
    spawn_mummy(ctx, (Vector2){.x = 9, .y = 3});
    spawn_mummy(ctx, (Vector2){.x = 4, .y = 5});
    spawn_mummy(ctx, (Vector2){.x = 2, .y = 2});
}

void spawn_third_wave(Context *ctx) {
    spawn_mummy(ctx, (Vector2){.x = 13, .y = 15});
    spawn_mummy(ctx, (Vector2){.x = 21, .y = 13});
    spawn_mummy(ctx, (Vector2){.x = 23, .y = 13});
    spawn_mummy(ctx, (Vector2){.x = 17, .y = 16});
    spawn_mummy(ctx, (Vector2){.x = 27, .y = 5});
    spawn_mummy(ctx, (Vector2){.x = 16, .y = 23});
}

void spawn_fourth_wave(Context *ctx) {
    spawn_mummy2(ctx, (Vector2){.x = 46, .y = 23});
    spawn_mummy2(ctx, (Vector2){.x = 46, .y = 25});
    spawn_mummy2(ctx, (Vector2){.x = 47, .y = 23});
    spawn_mummy2(ctx, (Vector2){.x = 43, .y = 26});
    spawn_mummy2(ctx, (Vector2){.x = 46, .y = 28});
}

void spawn_explosion(Context *ctx, Vector2 pos) {
    create_entity(ctx, create_explosion_level1_pos(pos, NULL));
}


void check_monster_spawns(Context *ctx) {
    Vector2 player_pos = ctx->camera.pos;

    if(trigger_second_wave_level1(player_pos)) {
        spawn_second_wave(ctx);
    }
    if(trigger_third_wave_level1(player_pos)) {
        spawn_third_wave(ctx);
    }
    if(trigger_fourth_wave_level1(player_pos)) {
        spawn_fourth_wave(ctx);
    }
    if(trigger_boss_level1(player_pos)) {
        spawn_boss(ctx);
    }
}

void init_mummy(YrEntity *self, void *data) {
    (void)data;
    EnemyData *md = calloc(1, sizeof(*md));
    md->hp = 100;
    md->hit_anim = MUMMY_HIT_ANIM;
    md->attack_anim = MUMMY_ATTACK_ANIM;
    md->damage = 5;
    self->entity_data = md;
}

void init_mummy2(YrEntity *self, void *data) {
    (void)data;
    EnemyData *ed = calloc(1, sizeof(*ed));
    ed->hp = 100;
    ed->hit_anim = MUMMY2_HIT_ANIM;
    ed->attack_anim = MUMMY2_ATTACK_ANIM;
    ed->damage = 10;
    self->entity_data = ed;
}

void init_boss(YrEntity *self, void *data) {
    (void)data;
    EnemyData *ed = calloc(1, sizeof(*ed));
    ed->hp = 500;
    ed->hit_anim = BOSS_HIT_ANIM;
    ed->attack_anim = BOSS_ATTACK_ANIM;
    ed->damage = 25;
    boss_spawn_animation = timer_start(2.0f);
    self->entity_data = ed;
}

void init_explosion(YrEntity *self, void *data) {
    (void)data;
    ExplosionData *ed = calloc(1, sizeof(*ed));
    ed->duration = timer_start(1.5f); 
    ed->damage_player=10; 
    ed->damage_enemy=30;
    self->entity_data = ed;
}

void init_boss_projectile(YrEntity *self, void *data) {
    ProjectileData * pj = (ProjectileData *)data;
    self->entity_data = calloc(1, sizeof(*pj));
    memcpy(self->entity_data, pj, sizeof(*pj));
}

void update_mummy(YrContext *ctx, YrEntity *self, size_t index) {
    EnemyData *data = (EnemyData *)self->entity_data;
    if (data->hp <= 0) {
        remove_entity(ctx, index);
        return;
    }

    float contact = PLAYER_COLLISION_THRESHOLD + self->collision_threshold * 2;

    if (self->dist > 0 && contact > self->dist) {
        if(timer_loop(&data->shot_cd, MUMMY_DAMAGE_COOLDOWN)) {
            damage_player(data->damage);
            start_animation_once(&self->animation, data->attack_anim);
        }
        return;
    }

    Vector2 dir = Vector2Subtract(ctx->camera.pos, self->pos);
    dir = Vector2Normalize(dir);
    Vector2 target = Vector2Add(self->pos, Vector2Scale(dir, MUMMY_SPEED * get_frame_time()));
    self->pos = slide_collision_out_radius(ctx, self->pos, target, NULL, self->collision_threshold, YR_CMSK_WALL | YR_CMSK_ENEMY, .5f);
}

void update_boss(YrContext *ctx, YrEntity *self, size_t index) {
    float x_slide = sinf(get_time() * 2.0f) * 0.5f;
    self->pos.x += x_slide * get_frame_time();

    EnemyData *data = (EnemyData *)self->entity_data;
    if (data->hp <= 0) {
        game.player.has_killed_boss = true;
        remove_entity(ctx, index);
        return;
    }

    if (timer_loop(&data->shot_cd, BOSS_SHOT_COOLDOWN)) {
        Vector2 dir = Vector2Subtract(ctx->camera.pos, self->pos);
        dir = Vector2Normalize(dir);
        Vector2 projectile_pos = Vector2Add(self->pos, Vector2Scale(dir, 0.1f));
        Entity e = create_boss_projectile_level1_pos(projectile_pos, NULL);
        ProjectileData pd = {.dir = dir, .damage = data->damage};
        create_entity_ex(ctx, e, &pd);
        start_animation_once(&self->animation, data->attack_anim);
    }
}

void update_boss_projectile(YrContext *ctx, YrEntity *self, size_t index) {
    ProjectileData *p = (ProjectileData *)self->entity_data;
    if (self->dist > 0 && self->dist < PLAYER_COLLISION_THRESHOLD + self->collision_threshold) {
        damage_player(p->damage);
        remove_entity(ctx, index);
        return;
    }
    self->pos = Vector2Add(self->pos, Vector2Scale(p->dir, PROJECTILE_SPEED * get_frame_time()));
    CollisionInfo hit = check_collision(ctx, self->pos, self->collision_threshold, YR_CMSK_WALL);
    if (hit.type == YR_COLLISION_WALL) {
        remove_entity(ctx, index);
        return;
    }
}

void pickup_gun(YrContext *ctx, YrEntity *self, size_t index) {
    if (self->dist < PLAYER_COLLISION_THRESHOLD + self->collision_threshold) {
        set_gun(WEP_GUN);
        remove_entity(ctx, index);
        spawn_first_wave(ctx);
    }
}

void pickup_shotgun(YrContext *ctx, YrEntity *self, size_t index) {
    if (self->dist < PLAYER_COLLISION_THRESHOLD + self->collision_threshold) {
        set_gun(WEP_BFG);
        remove_entity(ctx, index);
        spawn_first_wave(ctx);
    }
}

void update_explosion(YrContext *ctx, YrEntity *self, size_t index) {
    const float explosion_radius = 3.0f;
    ExplosionData *bomb = (ExplosionData *)self->entity_data;
    if (timer_is_done(&bomb->duration)) {
        remove_entity(ctx, index);
        return;
    }
    self->hdiv -= get_frame_time();
    bool in_player_range = self->dist > 0 && self->dist < PLAYER_COLLISION_THRESHOLD + explosion_radius;

    // damage
    if (!timer_loop(&bomb->damage_cd, 0.4f)) return;

    if (in_player_range) {
        damage_player(bomb->damage_player);
    }

    CollisionInfo hit[10];
    size_t cn = check_mult_collisions(ctx, self->pos, explosion_radius, YR_CMSK_ENEMY, hit, ARRAY_LEN(hit));
    size_t to_delete[10];
    int di = 0;
    for (size_t i = 0; i < cn; i++) {
        if (!hit[i].entity) continue;
        if (!hit[i].entity->entity_data) {
            to_delete[di++] = hit[i].entity_index;
            continue;
        }
        YrEntity *e = hit[i].entity;
        EnemyData *data = (EnemyData *)e->entity_data;
        data->hp -= bomb->damage_enemy;
        start_animation_once(&e->animation, data->hit_anim);
    }
    for (int i=0; i<di; i++) {
        remove_entity(ctx, to_delete[i]);
    }
}

void cleanup_data(Entity *e) {
    free(e->entity_data);
}

void move_player(Context *ctx) {
    Camera *p = &ctx->camera;
    float joy_x = esp_joystick_get_axis(joystick_id, YR_X_AXIS);
    float joy_y = esp_joystick_get_axis(joystick_id, YR_Y_AXIS);

    if (is_key_down(YR_KEY_A) || joy_x < -0.15f) {
        p->dir = rotate(p->dir, YR_COUNTERCLOCKWISE, PLAYER_ROTATION_SPEED);
    }
    if (is_key_down(YR_KEY_D) || joy_x > 0.15f) {
        p->dir = rotate(p->dir, YR_CLOCKWISE, PLAYER_ROTATION_SPEED);
    }

    Vector2 target = p->pos;
    bool is_moving = false;
    bool is_running = is_key_down(YR_KEY_R) || is_key_down(YR_KEY_LEFT_SHIFT);
    float speed = is_running ? PLAYER_RUN_SPEED : PLAYER_SPEED;
    if (is_key_down(YR_KEY_W) || joy_y > 0.15f) {
        is_moving = true;
        target = move(target, p->dir, YR_FORWARD, speed);
    }
    if (is_key_down(YR_KEY_S) || joy_y < -0.15f) {
        is_moving = true;
        target = move(target, p->dir, YR_BACK, speed);
    }
    if (is_key_down(YR_KEY_E)) {
        is_moving = true;
        target = move(target, p->dir, YR_RIGHT, speed);
    }
    if (is_key_down(YR_KEY_Q)) {
        is_moving = true;
        target = move(target, p->dir, YR_LEFT, speed);
    }

    CollisionInfo hit;
    p->pos = slide_collision(ctx, p->pos, target, &hit, PLAYER_COLLISION_THRESHOLD, YR_CMSK_PLAYER);

    if (is_moving) {
        game.player.bob_phase += get_frame_time() * (is_running ? PLAYER_RUN_BOB_SPEED : PLAYER_BOB_SPEED);
        game.player.bob_horizon = sinf(game.player.bob_phase) * (is_running ? PLAYER_RUN_BOB_AMOUNT : PLAYER_BOB_AMOUNT);
    } else {
        game.player.bob_phase = 0.0f;
        game.player.bob_horizon = Lerp(game.player.bob_horizon, 0.0f, get_frame_time() * 10.0f);
    }
}

void print_fps() {
    float fps = get_fps();
    char fps_text[32];
    sprintf(fps_text, "FPS: %.1f", fps);
    draw_text_ex(fps_text, fonts[YR_FONT_SM], YR_WHITE, .align=YR_LAY_TR, .x=10, .y=16);
}

void draw_hud(Context *ctx) {
    char hp_text[32];
    sprintf(hp_text, "HP: %d", game.player.hp);
    draw_text_ex(hp_text, fonts[YR_FONT_SM], YR_GREEN, .align=YR_LAY_TL, .x=10, .y=16);

    int gun_asset_id = 0;
    gun_asset_id = get_animation_texture(&game.player.gun.shot_animation);
    if(gun_asset_id<0) gun_asset_id = game.player.gun.tx_id;

    if (gun_asset_id) {
        draw_texture_ex(assets_map[gun_asset_id], 64, 128, .align=YR_LAY_CB, .height=ctx->screen_height * GUN_SCALE);
    }

    if (game.player.has_key) {
        draw_texture_ex(assets_map[tx_spr_092], 64, 64, .align=YR_LAY_BL);
    }
    print_fps();
}

void shoot_gun(Context *ctx) {
    if (!timer_is_done(&game.player.shot_timer)) return;

    GunData *gun = &game.player.gun;
    game.player.shot_timer = timer_start(gun->shot_cd);
    start_animation_once(&gun->shot_animation, gun->fire_anim);

    CollisionInfo hit = check_ray_collision(ctx, ctx->camera.pos, ctx->camera.dir, gun->range, YR_CMSK_ENEMY | YR_CMSK_DECORATION | YR_CMSK_WALL);
    if (hit.type != YR_COLLISION_ENTITY) return;

    if (hit.entity->kind == YR_KIND_EXPLOSIVE) {
        spawn_explosion(ctx, hit.entity->pos);
        remove_entity(ctx, hit.entity_index);
        return;
    }

    if (hit.entity->entity_data == NULL) {
        remove_entity(ctx, hit.entity_index);
        return;
    }

    EnemyData *data = (EnemyData *)hit.entity->entity_data;
    data->hp -= gun->damage;
    start_animation_once(&hit.entity->animation, data->hit_anim);
}

static inline void sepia_filter(int x, int y, yr_pixel_t *color, void *user_data) {
    (void)x;
    (void)y;
    (void)user_data;
#ifdef YR_RGB565
    int r = (*color >> 11) & 0x1F;
    int g = (*color >> 6) & 0x1F; // top 5 bits of green, same scale as r/b
    int b = (*color) & 0x1F;

    int nr = (101 * r + 197 * g + 48 * b) >> 8;
    int ng = (89 * r + 176 * g + 43 * b) >> 7; // back to 6-bit scale
    int nb = (70 * r + 137 * g + 34 * b) >> 8;
    if (nr > 31) nr = 31;
    if (ng > 63) ng = 63;
    if (nb > 31) nb = 31;
    *color = (yr_pixel_t)((nr << 11) | (ng << 5) | nb);
#else
    int r = (*color >> 24) & 0xFF;
    int g = (*color >> 16) & 0xFF;
    int b = (*color >> 8) & 0xFF;

    int nr = (101 * r + 197 * g + 48 * b) >> 8;
    int ng = (89 * r + 176 * g + 43 * b) >> 8;
    int nb = (70 * r + 137 * g + 34 * b) >> 8;
    if (nr > 255) nr = 255;
    if (ng > 255) ng = 255;
    if (nb > 255) nb = 255;
    *color = ((yr_pixel_t)nr << 24) | ((yr_pixel_t)ng << 16) | ((yr_pixel_t)nb << 8) | 0xFF;
#endif
}

static inline void red_vignette_filter(int x, int y, yr_pixel_t *color, void *user_data) {
    (void)user_data;

    int half_w = screen_width() / 2;
    int half_h = screen_height() / 2;
    int inv_w = (256 * 65536) / half_w;
    int inv_h = (256 * 65536) / half_h;

    int dx = ((x - half_w) * inv_w) >> 16;
    int dy = ((y - half_h) * inv_h) >> 16;
    int dist2 = (dx * dx + dy * dy) >> 8;

    int t = dist2 - 64;
    if (t < 0) t = 0;
    t >>= 1;
    if (t > 256) t = 256;

    int smooth = (t * t * (768 - 2 * t)) >> 16;
    int vignette = (smooth * 256) >> 8;

#ifdef YR_RGB565
    int r = (*color >> 11) & 0x1F;
    int g = (*color >> 5) & 0x3F;
    int b = (*color) & 0x1F;

    int nr = r + (((12 - r) * vignette) >> 8);
    int ng = (g * (256 - vignette)) >> 8;
    int nb = (b * (256 - vignette)) >> 8;
    if (nr < 0) nr = 0;
    if (nr > 31) nr = 31;
    if (ng > 63) ng = 63;
    if (nb > 31) nb = 31;
    *color = (yr_pixel_t)((nr << 11) | (ng << 5) | nb);
#else
    int r = (*color >> 24) & 0xFF;
    int g = (*color >> 16) & 0xFF;
    int b = (*color >> 8) & 0xFF;

    int nr = r + (((100 - r) * vignette) >> 8);
    int ng = (g * (256 - vignette)) >> 8;
    int nb = (b * (256 - vignette)) >> 8;
    if (nr < 0) nr = 0;
    if (nr > 255) nr = 255;
    if (ng > 255) ng = 255;
    if (nb > 255) nb = 255;
    *color = ((yr_pixel_t)nr << 24) | ((yr_pixel_t)ng << 16) | ((yr_pixel_t)nb << 8) | 0xFF;
#endif
}

void apply_screen_effects(Context *ctx) {
    if (!timer_is_done(&game.player.taking_damage)) {
        apply_color_filter(red_vignette_filter, NULL);
    }
    if (timer_is_started(&boss_spawn_animation) && !timer_is_done(&boss_spawn_animation)) {
        ctx->camera.horizon = sinf(get_time() * 100.0f) * 0.05f;
        apply_color_filter(sepia_filter, NULL);
    } else {
        ctx->camera.horizon = game.player.bob_horizon;
    }
}

void reset_game(Context *ctx) {
    game.state = GAME_MENU;
    game.player.hp = 100;
    da_free(&game.player.gun.shot_animation);
    set_gun(WEP_HND);
    game.player.has_key = false;
    game.player.has_killed_boss = false;
    game.player.bob_phase = 0.0f;
    game.player.bob_horizon = 0.0f;
    memset(&boss_spawn_animation, 0, sizeof(boss_spawn_animation));
    while (ctx->entities.length > 0) {
        remove_entity(ctx, ctx->entities.data[0].key);
    }
}

// Main game functions
void yr_init_game(Context *ctx) {
    ctx->screen_width = 960;
    ctx->screen_height = 544;
    ctx->target_fps = TARGET_FPS;

    joystick_id = esp_joystick_init(32, 36);
    esp_key_init(25, YR_KEY_Q);
    esp_key_init(2, YR_KEY_E);
    esp_key_init(15, YR_KEY_R);
    esp_key_init(26, YR_KEY_SPACE);

    reset_game(ctx);
}

void yr_update_game(Context *ctx) {
    switch (game.state) {
    case GAME_MENU: {
        clear_screen(YR_BLACK);
        draw_text_ex("YARI FPS", fonts[YR_FONT_MD], YR_WHITE, .align=YR_LAY_CENTER, .y=-16);
        draw_text_ex("Press SPACE to start", fonts[YR_FONT_SM], YR_WHITE, .align=YR_LAY_CENTER, .y=16);
        if (is_key_pressed(YR_KEY_SPACE)) {
            game.state = GAME_LEVEL1;
            load_level1(ctx);
        }
    } break;
    case GAME_LEVEL1: {
        draw_game();
        move_player(ctx);

        check_monster_spawns(ctx);

        if (is_key_down(YR_KEY_SPACE)) {
            shoot_gun(ctx);
        }

        if (game.player.hp <= 0) {
            game.state = GAME_OVER;
        }
        apply_screen_effects(ctx);
        draw_hud(ctx);
    } break;
    case GAME_MENU2: {
        clear_screen(YR_BLACK);
        draw_text_ex("Level 2", fonts[YR_FONT_MD], YR_WHITE, .align=YR_LAY_CENTER, .y=-16);
        draw_text_ex("Press SPACE to start", fonts[YR_FONT_SM], YR_WHITE, .align=YR_LAY_CENTER, .y=16);
        if (is_key_pressed(YR_KEY_SPACE)) {
            reset_game(ctx);
            game.state = GAME_LEVEL2;
            load_level2(ctx);
        }
    } break;
    case GAME_LEVEL2: {
        draw_game();
        move_player(ctx);

        if (is_key_down(YR_KEY_SPACE)) {
            shoot_gun(ctx);
        }

        if (game.player.hp <= 0) {
            game.state = GAME_OVER;
        }
        if (ctx->entities.length == 0) {
            game.state = GAME_WIN;
            return;
        }
        apply_screen_effects(ctx);
        draw_hud(ctx);
    } break;
    case GAME_WIN: {
        clear_screen(YR_BLACK);
        draw_text_ex("You win!", fonts[YR_FONT_MD], YR_WHITE, .align=YR_LAY_CENTER, .y=-16);
        draw_text_ex("Press R to restart", fonts[YR_FONT_SM], YR_WHITE, .align=YR_LAY_CENTER, .y=16);
        if (is_key_pressed(YR_KEY_R)) {
            reset_game(ctx);
        }
    } break;
    case GAME_OVER: {
        clear_screen(YR_BLACK);
        draw_text_ex("Game Over", fonts[YR_FONT_MD], YR_WHITE, .align=YR_LAY_CENTER, .y=-16);
        draw_text_ex("Press R to restart", fonts[YR_FONT_SM], YR_WHITE, .align=YR_LAY_CENTER, .y=16);
        if (is_key_pressed(YR_KEY_R)) {
            reset_game(ctx);
        }
    } break;
    }
}
