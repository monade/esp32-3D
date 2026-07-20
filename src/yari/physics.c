
#include "physics.h"
#include "da.h"

static const float RAY_EPSILON = 0.0001f;

// Physical thickness given to the otherwise-zero-thickness THIN_H/THIN_V/
// THIN_D1/THIN_D2/THIN_X walls for collision purposes - a true zero-width
// plane would let players clip through via discrete position sampling.
static const float THIN_WALL_HALF_THICKNESS = 0.05f;

static float dist_point_segment(Vector2 p, Vector2 a, Vector2 b) {
    float abx = b.x - a.x;
    float aby = b.y - a.y;
    float len_sqr = abx * abx + aby * aby;
    float t = len_sqr > 0.0f ? ((p.x - a.x) * abx + (p.y - a.y) * aby) / len_sqr : 0.0f;
    if (t < 0.0f) t = 0.0f;
    else if (t > 1.0f) t = 1.0f;
    float cx = a.x + abx * t;
    float cy = a.y + aby * t;
    float dx = p.x - cx;
    float dy = p.y - cy;
    return sqrtf(dx * dx + dy * dy);
}

// Whether `point` actually falls inside tile's solid geometry, matching the
// exact model the renderer uses (yr_raycast_walls in yari.c) rather than
// just "is this cell non-empty" - a receded/thin/diagonal wall only
// occupies part of its cell.
static bool point_in_wall(const YrWall *tile, size_t cell_x, size_t cell_y, Vector2 point) {
    if (tile->kind == YR_WK_EMPTY) return false;

    if (tile->kind == YR_WK_THIN_D1 || tile->kind == YR_WK_THIN_D2 || tile->kind == YR_WK_THIN_X) {
        Vector2 a, b;
        if (tile->kind == YR_WK_THIN_D1 || tile->kind == YR_WK_THIN_X) {
            yr__wall_diagonal_endpoints(tile, cell_x, cell_y, false, &a, &b);
            if (dist_point_segment(point, a, b) <= THIN_WALL_HALF_THICKNESS) return true;
        }
        if (tile->kind == YR_WK_THIN_D2 || tile->kind == YR_WK_THIN_X) {
            yr__wall_diagonal_endpoints(tile, cell_x, cell_y, true, &a, &b);
            if (dist_point_segment(point, a, b) <= THIN_WALL_HALF_THICKNESS) return true;
        }
        return false;
    }

    float x0, x1, y0, y1;
    if (tile->kind == YR_WK_THIN_H || tile->kind == YR_WK_THIN_V) {
        yr__wall_thin_bar_box(tile, cell_x, cell_y, &x0, &x1, &y0, &y1);
        if (tile->kind == YR_WK_THIN_H) { y0 -= THIN_WALL_HALF_THICKNESS; y1 += THIN_WALL_HALF_THICKNESS; }
        else { x0 -= THIN_WALL_HALF_THICKNESS; x1 += THIN_WALL_HALF_THICKNESS; }
    } else {
        yr__wall_recede_box(tile, cell_x, cell_y, &x0, &x1, &y0, &y1);
    }
    return point.x >= x0 && point.x <= x1 && point.y >= y0 && point.y <= y1;
}

// Ray-vs-wall test mirroring yr_raycast_walls' box/diagonal test, used by
// the ray DDA below so it only stops at a cell once the ray actually
// crosses that wall's (possibly receded/thin/diagonal) solid geometry,
// rather than at the raw grid boundary.
static bool ray_hits_wall(const YrWall *tile, size_t cell_x, size_t cell_y, Vector2 origin, Vector2 dir, float *out_t) {
    if (tile->kind == YR_WK_THIN_D1 || tile->kind == YR_WK_THIN_D2 || tile->kind == YR_WK_THIN_X) {
        float best_t = -1.0f;
        float t, s;
        bool side;
        Vector2 a, b;
        if (tile->kind == YR_WK_THIN_D1 || tile->kind == YR_WK_THIN_X) {
            yr__wall_diagonal_endpoints(tile, cell_x, cell_y, false, &a, &b);
            if (yr__thin_diagonal_hit(origin, dir, a.x, a.y, b.x, b.y, &t, &s, &side) && (best_t < 0.0f || t < best_t)) best_t = t;
        }
        if (tile->kind == YR_WK_THIN_D2 || tile->kind == YR_WK_THIN_X) {
            yr__wall_diagonal_endpoints(tile, cell_x, cell_y, true, &a, &b);
            if (yr__thin_diagonal_hit(origin, dir, a.x, a.y, b.x, b.y, &t, &s, &side) && (best_t < 0.0f || t < best_t)) best_t = t;
        }
        if (best_t < 0.0f) return false;
        *out_t = best_t;
        return true;
    }

    float x0, x1, y0, y1;
    if (tile->kind == YR_WK_THIN_H || tile->kind == YR_WK_THIN_V) {
        yr__wall_thin_bar_box(tile, cell_x, cell_y, &x0, &x1, &y0, &y1);
    } else {
        yr__wall_recede_box(tile, cell_x, cell_y, &x0, &x1, &y0, &y1);
    }

    float tx1 = (x0 - origin.x) / dir.x;
    float tx2 = (x1 - origin.x) / dir.x;
    float ty1 = (y0 - origin.y) / dir.y;
    float ty2 = (y1 - origin.y) / dir.y;
    float tx_min = tx1 < tx2 ? tx1 : tx2;
    float tx_max = tx1 < tx2 ? tx2 : tx1;
    float ty_min = ty1 < ty2 ? ty1 : ty2;
    float ty_max = ty1 < ty2 ? ty2 : ty1;
    float t_enter = tx_min > ty_min ? tx_min : ty_min;
    float t_exit = tx_max < ty_max ? tx_max : ty_max;
    if (t_enter > t_exit || t_exit < 0.0f) return false;

    *out_t = t_enter > 0.0f ? t_enter : 0.0f;
    return true;
}

static YrCollisionInfo check_wall_at(YrContext *ctx, Vector2 point) {
    YrCollisionInfo info = {0};
    if (point.x < 0.0f || point.y < 0.0f) return info;

    size_t cell_x = (size_t)point.x;
    size_t cell_y = (size_t)point.y;
    if (cell_x >= ctx->map.cols || cell_y >= ctx->map.rows) return info;

    YrWall *tile = &ctx->map.walls[cell_y * ctx->map.cols + cell_x];
    if (point_in_wall(tile, cell_x, cell_y, point)) {
        info.type = YR_COLLISION_WALL;
        info.cell_x = (int)cell_x;
        info.cell_y = (int)cell_y;
        info.tile = *tile;
    }
    return info;
}

static YrCollisionInfo check_wall_ray_collision(YrContext *ctx, Vector2 origin, Vector2 dir, float max_dist, float *hit_dist) {
    YrCollisionInfo info = {0};

    if (fabsf(dir.x) < RAY_EPSILON) dir.x = (dir.x < 0.0f) ? -RAY_EPSILON : RAY_EPSILON;
    if (fabsf(dir.y) < RAY_EPSILON) dir.y = (dir.y < 0.0f) ? -RAY_EPSILON : RAY_EPSILON;

    size_t map_x = (size_t)floorf(origin.x);
    size_t map_y = (size_t)floorf(origin.y);

    float delta_x = fabsf(1.0f / dir.x);
    float delta_y = fabsf(1.0f / dir.y);
    float side_dist_x;
    float side_dist_y;
    int step_x;
    int step_y;

    if (dir.x < 0.0f) {
        step_x = -1;
        side_dist_x = (origin.x - (float)map_x) * delta_x;
    } else {
        step_x = 1;
        side_dist_x = ((float)map_x + 1.0f - origin.x) * delta_x;
    }

    if (dir.y < 0.0f) {
        step_y = -1;
        side_dist_y = (origin.y - (float)map_y) * delta_y;
    } else {
        step_y = 1;
        side_dist_y = ((float)map_y + 1.0f - origin.y) * delta_y;
    }

    // THIN_H/THIN_V/THIN_D1/THIN_D2/THIN_X can sit inside the ray's own
    // starting cell (e.g. standing right next to a thin divider), so that
    // cell needs testing once, against the actual ray, before the DDA below
    // starts stepping away from it - a plain point check at `origin` isn't
    // enough since the ray can cross the wall's geometry further into the
    // cell without origin itself touching it.
    bool first_cell = true;

    while (true) {
        if (first_cell) {
            first_cell = false;
            if (map_x >= ctx->map.cols || map_y >= ctx->map.rows) continue;
        } else {
            if (side_dist_x < side_dist_y) {
                if (side_dist_x > max_dist) break;
                map_x += step_x;
                side_dist_x += delta_x;
            } else {
                if (side_dist_y > max_dist) break;
                map_y += step_y;
                side_dist_y += delta_y;
            }

            if (map_x >= ctx->map.cols || map_y >= ctx->map.rows) break;
        }

        YrWall *tile = &ctx->map.walls[map_y * ctx->map.cols + map_x];
        if (tile->kind == YR_WK_EMPTY) continue;

        float t;
        if (ray_hits_wall(tile, map_x, map_y, origin, dir, &t) && t <= max_dist) {
            *hit_dist = t;
            info.type = YR_COLLISION_WALL;
            info.cell_x = (int)map_x;
            info.cell_y = (int)map_y;
            info.tile = *tile;
            return info;
        }
    }

    return (YrCollisionInfo){0};
}

size_t yr_check_mult_collisions_out_radius(YrContext *ctx, Vector2 next_pos, float threshold, uint32_t collision_mask, float radius, YrCollisionInfo *out_info, size_t len) {
    size_t count = 0;
    YrCollisionInfo info = {0};

    // entities
    if (collision_mask & ~YR_CMSK_WALL) {
        for (size_t i = 0; i < ctx->entities.length; i++) {
            YrEntity *sprite = &ctx->entities.data[i].value;
            if (sprite->disabled) continue;
            if (!(sprite->collision_mask & collision_mask)) continue;
            float dist_sqr = Vector2DistanceSqr(next_pos, sprite->pos);
            float reach = sprite->collision_threshold + threshold;
            if (dist_sqr < reach * reach && dist_sqr > radius * radius) {
                info.type = YR_COLLISION_ENTITY;
                info.entity = sprite;
                info.entity_index = ctx->entities.data[i].key;
                if (out_info && count < len) {
                    out_info[count] = info;
                    if (count + 1 >= len) return len;
                }
                count++;
            }
        }
    }

    // walls
    if (collision_mask & YR_CMSK_WALL) {
        Vector2 samples[] = {
            next_pos,
            { next_pos.x + threshold, next_pos.y },
            { next_pos.x - threshold, next_pos.y },
            { next_pos.x, next_pos.y + threshold },
            { next_pos.x, next_pos.y - threshold },
        };
        for (size_t i = 0; i < YR_ARRAY_LEN(samples); i++) {
            info = check_wall_at(ctx, samples[i]);
            if (info.type != YR_COLLISION_NONE) {
                if (out_info && count < len) {
                    out_info[count] = info;
                    if (count + 1 >= len) return len;
                }
                count++;
            }
        }
    }
    return count;
}

YrCollisionInfo yr_check_collision_out_radius(YrContext *ctx, Vector2 next_pos, float threshold, uint32_t collision_mask, float radius) {
    YrCollisionInfo info = {0};
    yr_check_mult_collisions_out_radius(ctx, next_pos, threshold, collision_mask, radius, &info, 1);
    return info;
}

YrCollisionInfo yr_check_ray_collision(YrContext *ctx, Vector2 origin, Vector2 dir, float threshold, uint32_t collision_mask) {
    YrCollisionInfo info = {0};
    if (threshold <= 0.0f) return info;

    float dir_len = Vector2Length(dir);
    if (dir_len < RAY_EPSILON) return info;
    dir = Vector2Scale(dir, 1.0f / dir_len);

    float best_dist = threshold;
    bool has_hit = false;

    if (collision_mask & ~YR_CMSK_WALL) {
        for (size_t i = 0; i < ctx->entities.length; i++) {
            YrEntity *sprite = &ctx->entities.data[i].value;
            if (sprite->disabled) continue;
            if (!(sprite->collision_mask & collision_mask)) continue;

            float radius = sprite->collision_threshold;
            if (radius < 0.0f) radius = 0.0f;

            Vector2 rel = Vector2Subtract(sprite->pos, origin);
            float projection = Vector2DotProduct(rel, dir);
            float rel_len_sqr = Vector2LengthSqr(rel);
            float closest_dist_sqr = rel_len_sqr - projection * projection;
            if (closest_dist_sqr < 0.0f) closest_dist_sqr = 0.0f;

            float radius_sqr = radius * radius;
            if (closest_dist_sqr > radius_sqr) continue;

            float offset = sqrtf(radius_sqr - closest_dist_sqr);
            float hit_dist = projection - offset;
            if (hit_dist < 0.0f) {
                if (projection + offset < 0.0f) continue;
                hit_dist = 0.0f;
            }

            if (hit_dist <= threshold && (!has_hit || hit_dist < best_dist)) {
                info.type = YR_COLLISION_ENTITY;
                info.entity = sprite;
                info.entity_index = ctx->entities.data[i].key;
                best_dist = hit_dist;
                has_hit = true;
            }
        }
    }

    if (collision_mask & YR_CMSK_WALL) {
        float wall_dist = 0.0f;
        YrCollisionInfo wall = check_wall_ray_collision(ctx, origin, dir, threshold, &wall_dist);
        if (wall.type != YR_COLLISION_NONE && (!has_hit || wall_dist < best_dist)) {
            info = wall;
        }
    }

    return info;
}

Vector2 yr_slide_collision(YrContext *ctx, Vector2 from, Vector2 to, YrCollisionInfo *hit, float threshold, uint32_t collision_mask) {
    return yr_slide_collision_out_radius(ctx, from, to, hit, threshold, collision_mask, 0);
}

static bool path_crosses_wall(YrContext *ctx, Vector2 from, Vector2 candidate, float threshold) {
    Vector2 delta = Vector2Subtract(candidate, from);
    float dist = Vector2Length(delta);
    if (dist <= RAY_EPSILON) return false;
    Vector2 dir = Vector2Scale(delta, 1.0f / dist);
    for (float t = THIN_WALL_HALF_THICKNESS; t < dist; t += THIN_WALL_HALF_THICKNESS) {
        Vector2 p = Vector2Add(from, Vector2Scale(dir, t));
        if (yr_check_collision_out_radius(ctx, p, threshold, YR_CMSK_WALL, 0.0f).type != YR_COLLISION_NONE) return true;
    }
    return false;
}

Vector2 yr_slide_collision_out_radius(YrContext *ctx, Vector2 from, Vector2 to, YrCollisionInfo *hit, float threshold, uint32_t collision_mask, float radius) {
    bool check_tunneling = (collision_mask & YR_CMSK_WALL) != 0;
    YrCollisionInfo info = yr_check_collision_out_radius(ctx, to, threshold, collision_mask, radius);
    if (hit) *hit = info;
    if (info.type == YR_COLLISION_NONE && !(check_tunneling && path_crosses_wall(ctx, from, to, threshold))) return to;

    // A diagonal wall's natural slide direction is along its own 45-degree
    // line, not axis-aligned - trying only the axis-aligned candidates below
    // (as if brushing against a straight FULL/THIN_H/THIN_V face) produces a
    // visible stair-step jitter right at the surface. Project the motion
    // onto the diagonal's tangent instead and use that if it clears.
    if (info.type == YR_COLLISION_WALL &&
        (info.tile.kind == YR_WK_THIN_D1 || info.tile.kind == YR_WK_THIN_D2 || info.tile.kind == YR_WK_THIN_X)) {
        Vector2 delta = Vector2Subtract(to, from);
        bool slashes[2];
        int slash_count = 0;
        if (info.tile.kind == YR_WK_THIN_D1 || info.tile.kind == YR_WK_THIN_X) slashes[slash_count++] = false;
        if (info.tile.kind == YR_WK_THIN_D2 || info.tile.kind == YR_WK_THIN_X) slashes[slash_count++] = true;

        for (int i = 0; i < slash_count; i++) {
            Vector2 a, b;
            yr__wall_diagonal_endpoints(&info.tile, (size_t)info.cell_x, (size_t)info.cell_y, slashes[i], &a, &b);
            Vector2 tangent = Vector2Normalize(Vector2Subtract(b, a));
            Vector2 candidate = Vector2Add(from, Vector2Scale(tangent, Vector2DotProduct(delta, tangent)));
            if (yr_check_collision_out_radius(ctx, candidate, threshold, collision_mask, radius).type == YR_COLLISION_NONE &&
                !(check_tunneling && path_crosses_wall(ctx, from, candidate, threshold))) {
                return candidate;
            }
        }
    }

    Vector2 slide_x = { to.x, from.y };
    if (yr_check_collision_out_radius(ctx, slide_x, threshold, collision_mask, radius).type == YR_COLLISION_NONE &&
        !(check_tunneling && path_crosses_wall(ctx, from, slide_x, threshold))) {
        return slide_x;
    }
    Vector2 slide_y = { from.x, to.y };
    if (yr_check_collision_out_radius(ctx, slide_y, threshold, collision_mask, radius).type == YR_COLLISION_NONE &&
        !(check_tunneling && path_crosses_wall(ctx, from, slide_y, threshold))) {
        return slide_y;
    }
    return from;
}

Vector2 yr_rotate(Vector2 vector, enum YrRotationDirection direction, float rotation_speed) {
    float dt = yr_get_frame_time();
    if (direction == YR_COUNTERCLOCKWISE) {
        dt *= -1;
    }
    return Vector2Rotate(vector, dt * rotation_speed);
}

Vector2 yr_move(Vector2 subject_position, Vector2 subject_direction, enum YrMovementDirection direction, float movement_speed) {
    float dt = yr_get_frame_time();
    if (direction == YR_BACK) {
        dt *= -1;
    }
    if (direction == YR_LEFT || direction == YR_RIGHT) {
        subject_direction = Vector2Rotate(subject_direction, direction == YR_LEFT ? -PI/2 : PI/2);
    }
    return Vector2Add(subject_position, Vector2Scale(subject_direction, dt*movement_speed));
}
