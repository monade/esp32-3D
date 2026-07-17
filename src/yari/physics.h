#ifndef YR_PHYSICS_H
#define YR_PHYSICS_H
#include <stdint.h>
#include "yari.h"

enum YrMovementDirection {
  YR_FORWARD,
  YR_BACK,
  YR_LEFT,
  YR_RIGHT
};

enum YrRotationDirection {
  YR_CLOCKWISE,
  YR_COUNTERCLOCKWISE
};

typedef enum {
  YR_COLLISION_NONE = 0,
  YR_COLLISION_WALL,
  YR_COLLISION_ENTITY
} YrCollisionType;

typedef struct {
  YrCollisionType type;
  union {
    struct {
        int cell_x;
        int cell_y;
        YrWall tile;
    };
    struct {
        YrEntity *entity;
        size_t entity_index;
    };
  };
} YrCollisionInfo;

size_t yr_check_mult_collisions_out_radius(YrContext *ctx, Vector2 next_pos, float threshold, uint32_t collision_mask, float radius, YrCollisionInfo *out_info, size_t len);
YrCollisionInfo yr_check_collision_out_radius(YrContext *ctx, Vector2 next_pos, float threshold, uint32_t collision_mask, float radius);
#define yr_check_mult_collisions(state, next_pos, threshold, collision_mask, out_info, len) \
    yr_check_mult_collisions_out_radius(state, next_pos, threshold, collision_mask, 0.0f, out_info, len)
#define yr_check_collision(state, next_pos, threshold, collision_mask) \
    yr_check_collision_out_radius(state, next_pos, threshold, collision_mask, 0.0f)

YrCollisionInfo yr_check_ray_collision(YrContext *ctx, Vector2 origin, Vector2 dir, float threshold, uint32_t collision_mask);

Vector2 yr_slide_collision(YrContext *ctx, Vector2 from, Vector2 to, YrCollisionInfo *hit, float threshold, uint32_t collision_mask);
Vector2 yr_slide_collision_out_radius(YrContext *ctx, Vector2 from, Vector2 to, YrCollisionInfo *hit, float threshold, uint32_t collision_mask, float radius);


Vector2 yr_rotate(Vector2 vector, enum YrRotationDirection direction, float rotation_speed);

Vector2 yr_move(Vector2 subject_position, Vector2 subject_direction, enum YrMovementDirection direction, float movement_speed);

#ifdef YARI_NO_PREFIX
#define CollisionInfo YrCollisionInfo
#define check_collision yr_check_collision
#define check_collision_out_radius yr_check_collision_out_radius
#define check_mult_collisions yr_check_mult_collisions
#define check_mult_collisions_out_radius yr_check_mult_collisions_out_radius
#define check_ray_collision yr_check_ray_collision
#define slide_collision yr_slide_collision
#define slide_collision_out_radius yr_slide_collision_out_radius
#define rotate yr_rotate
#define move yr_move
#endif

#endif // YR_PHYSICS_H
