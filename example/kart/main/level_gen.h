// File generated automatically by map_builder.c. DO NOT EDIT.
// MAP_BUILDER_STATE_BEGIN
// version 2
// player_collision 0.300000012 0xFFFFFFFB
// layer ENTITY 1
// layer DROP 2
// MAP_BUILDER_STATE_END

#ifndef YR_LEVEL_GEN_H
#define YR_LEVEL_GEN_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <yari.h>
#include "assets.h"

#define YR_CMSK_ENTITY (1u << 1)
#define YR_CMSK_DROP (1u << 2)
#define YR_CMSK_PLAYER (YR_CMSK_ALL & ~YR_CMSK_DROP)

#define YR_KIND_DEFAULT 0

#define YR_PLAYER_COLLISION_THRESHOLD 0.300000

#endif // YR_LEVEL_GEN_H
