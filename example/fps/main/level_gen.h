// File generated automatically by map_builder.c. DO NOT EDIT.
// MAP_BUILDER_STATE_BEGIN
// version 2
// player_collision 0.150000006 0xFFFFFFCD
// layer PICKUP 1
// layer ENEMY 2
// layer DECORATION 3
// layer PROJECTILE 4
// init_fn init_mummy
// init_fn init_explosion
// init_fn init_mummy2
// init_fn init_boss
// init_fn init_boss_projectile
// update_fn pickup_key
// update_fn trigger_end
// update_fn pickup_gun
// update_fn update_mummy
// update_fn pickup_shotgun
// update_fn update_boss
// update_fn update_boss_projectile
// update_fn pickup_medikit
// update_fn update_explosion
// cleanup_fn cleanup_data
// kind_def EXPLOSIVE 1
// anim mummy 0.25 2 tx_spr_009 tx_spr_010
// anim boss 0.25 2 tx_spr_056 tx_spr_059
// anim mummy_attack 0.25 3 tx_spr_007 tx_spr_009 tx_spr_010
// anim mummy_hit 0.25 2 tx_spr_007 tx_spr_008
// anim boss_attack 0.25 2 tx_spr_060 tx_spr_057
// anim boss_hit 0.25 2 tx_spr_056 tx_spr_058
// anim mummy2 0.25 2 tx_spr_049 tx_spr_052
// anim mummy2_hit 0.25 2 tx_spr_049 tx_spr_051
// anim mummy2_attack 0.25 2 tx_spr_050 tx_spr_052
// anim dummy1 0.25 4 tx_spr_066 tx_spr_062 tx_spr_063 tx_spr_062
// anim dummy2 0.25 6 tx_spr_069 tx_spr_072 tx_spr_070 tx_spr_073 tx_spr_070 tx_spr_072
// anim dummy3 0.25 4 tx_spr_024 tx_spr_025 tx_spr_027 tx_spr_028
// anim dummy4 0.25 5 tx_spr_030 tx_spr_034 tx_spr_031 tx_spr_033 tx_spr_031
// anim dummy5 0.25 4 tx_spr_080 tx_spr_081 tx_spr_081b tx_spr_081
// MAP_BUILDER_STATE_END

#ifndef YR_LEVEL_GEN_H
#define YR_LEVEL_GEN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <yari.h>
#include "assets.h"

#define YR_CMSK_PICKUP (1u << 1)
#define YR_CMSK_ENEMY (1u << 2)
#define YR_CMSK_DECORATION (1u << 3)
#define YR_CMSK_PROJECTILE (1u << 4)
#define YR_CMSK_PLAYER (YR_CMSK_ALL & ~YR_CMSK_PICKUP & ~YR_CMSK_PROJECTILE)

#define YR_KIND_DEFAULT 0
#define YR_KIND_EXPLOSIVE 1

#define YR_PLAYER_COLLISION_THRESHOLD 0.150000

void init_mummy(YrEntity *self, void *data);
void init_explosion(YrEntity *self, void *data);
void init_mummy2(YrEntity *self, void *data);
void init_boss(YrEntity *self, void *data);
void init_boss_projectile(YrEntity *self, void *data);

void pickup_key(YrGameState *state, YrEntity *self, size_t index);
void trigger_end(YrGameState *state, YrEntity *self, size_t index);
void pickup_gun(YrGameState *state, YrEntity *self, size_t index);
void update_mummy(YrGameState *state, YrEntity *self, size_t index);
void pickup_shotgun(YrGameState *state, YrEntity *self, size_t index);
void update_boss(YrGameState *state, YrEntity *self, size_t index);
void update_boss_projectile(YrGameState *state, YrEntity *self, size_t index);
void pickup_medikit(YrGameState *state, YrEntity *self, size_t index);
void update_explosion(YrGameState *state, YrEntity *self, size_t index);

void cleanup_data(YrEntity *self);

static const int MUMMY_ANIM_FRAMES[] = {tx_spr_009, tx_spr_010};
static const YrAnimation MUMMY_ANIM = {
    .frames = MUMMY_ANIM_FRAMES,
    .frame_count = 2,
    .duration = 0.250000f,
};

static const int BOSS_ANIM_FRAMES[] = {tx_spr_056, tx_spr_059};
static const YrAnimation BOSS_ANIM = {
    .frames = BOSS_ANIM_FRAMES,
    .frame_count = 2,
    .duration = 0.250000f,
};

static const int MUMMY_ATTACK_ANIM_FRAMES[] = {tx_spr_007, tx_spr_009, tx_spr_010};
static const YrAnimation MUMMY_ATTACK_ANIM = {
    .frames = MUMMY_ATTACK_ANIM_FRAMES,
    .frame_count = 3,
    .duration = 0.250000f,
};

static const int MUMMY_HIT_ANIM_FRAMES[] = {tx_spr_007, tx_spr_008};
static const YrAnimation MUMMY_HIT_ANIM = {
    .frames = MUMMY_HIT_ANIM_FRAMES,
    .frame_count = 2,
    .duration = 0.250000f,
};

static const int BOSS_ATTACK_ANIM_FRAMES[] = {tx_spr_060, tx_spr_057};
static const YrAnimation BOSS_ATTACK_ANIM = {
    .frames = BOSS_ATTACK_ANIM_FRAMES,
    .frame_count = 2,
    .duration = 0.250000f,
};

static const int BOSS_HIT_ANIM_FRAMES[] = {tx_spr_056, tx_spr_058};
static const YrAnimation BOSS_HIT_ANIM = {
    .frames = BOSS_HIT_ANIM_FRAMES,
    .frame_count = 2,
    .duration = 0.250000f,
};

static const int MUMMY2_ANIM_FRAMES[] = {tx_spr_049, tx_spr_052};
static const YrAnimation MUMMY2_ANIM = {
    .frames = MUMMY2_ANIM_FRAMES,
    .frame_count = 2,
    .duration = 0.250000f,
};

static const int MUMMY2_HIT_ANIM_FRAMES[] = {tx_spr_049, tx_spr_051};
static const YrAnimation MUMMY2_HIT_ANIM = {
    .frames = MUMMY2_HIT_ANIM_FRAMES,
    .frame_count = 2,
    .duration = 0.250000f,
};

static const int MUMMY2_ATTACK_ANIM_FRAMES[] = {tx_spr_050, tx_spr_052};
static const YrAnimation MUMMY2_ATTACK_ANIM = {
    .frames = MUMMY2_ATTACK_ANIM_FRAMES,
    .frame_count = 2,
    .duration = 0.250000f,
};

static const int DUMMY1_ANIM_FRAMES[] = {tx_spr_066, tx_spr_062, tx_spr_063, tx_spr_062};
static const YrAnimation DUMMY1_ANIM = {
    .frames = DUMMY1_ANIM_FRAMES,
    .frame_count = 4,
    .duration = 0.250000f,
};

static const int DUMMY2_ANIM_FRAMES[] = {tx_spr_069, tx_spr_072, tx_spr_070, tx_spr_073, tx_spr_070, tx_spr_072};
static const YrAnimation DUMMY2_ANIM = {
    .frames = DUMMY2_ANIM_FRAMES,
    .frame_count = 6,
    .duration = 0.250000f,
};

static const int DUMMY3_ANIM_FRAMES[] = {tx_spr_024, tx_spr_025, tx_spr_027, tx_spr_028};
static const YrAnimation DUMMY3_ANIM = {
    .frames = DUMMY3_ANIM_FRAMES,
    .frame_count = 4,
    .duration = 0.250000f,
};

static const int DUMMY4_ANIM_FRAMES[] = {tx_spr_030, tx_spr_034, tx_spr_031, tx_spr_033, tx_spr_031};
static const YrAnimation DUMMY4_ANIM = {
    .frames = DUMMY4_ANIM_FRAMES,
    .frame_count = 5,
    .duration = 0.250000f,
};

static const int DUMMY5_ANIM_FRAMES[] = {tx_spr_080, tx_spr_081, tx_spr_081b, tx_spr_081};
static const YrAnimation DUMMY5_ANIM = {
    .frames = DUMMY5_ANIM_FRAMES,
    .frame_count = 4,
    .duration = 0.250000f,
};

#endif // YR_LEVEL_GEN_H
