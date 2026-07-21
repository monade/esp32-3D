#include "yari.h"

#include <stdlib.h>

#define THRESHOLD 0.0001

static int compare_sprite_dist(const void *a, const void *b) {
    const YrRenderSprite *sa = (const YrRenderSprite *)a;
    const YrRenderSprite *sb = (const YrRenderSprite *)b;
    if (sa->dist < sb->dist) return 1;
    if (sa->dist > sb->dist) return -1;
    return 0;
}

static inline int fixed16_to_int(int value) {
    return value >= 0 ? (value >> 16) : -((-value) >> 16);
}

// Ray-vs-diagonal-segment test used by THIN_D1/THIN_D2/THIN_X: the segment
// runs from (ex0,ey0) to (ex1,ey1) (see yr__wall_diagonal_endpoints for how
// those are derived). Returns false if the ray misses the segment or runs
// parallel to it; otherwise fills *out_t (ray distance), *out_s (0..1
// position along the segment, used directly as the texture coordinate) and
// *out_side (which face of the segment the ray approaches from, for
// texture mirroring).
bool YR_PERF_ATTR yr__thin_diagonal_hit(Vector2 pos, Vector2 dir, float ex0, float ey0, float ex1, float ey1, float *out_t, float *out_s, bool *out_side) {
    float ddx = ex1 - ex0;
    float ddy = ey1 - ey0;
    float denom = dir.x * ddy - dir.y * ddx;
    if (denom > -THRESHOLD && denom < THRESHOLD) return false; // ray runs parallel to the diagonal

    float t = ((pos.y - ey0) * ddx - (pos.x - ex0) * ddy) / denom;
    if (t < 0.0f) return false;

    float s = fabsf(ddx) > fabsf(ddy) ? (pos.x + dir.x * t - ex0) / ddx : (pos.y + dir.y * t - ey0) / ddy;
    if (s < 0.0f || s > 1.0f) return false;

    *out_t = t;
    *out_s = s;
    *out_side = denom < 0.0f;
    return true;
}

// The zero-thickness diagonal endpoints for THIN_D1 (slash=false, nominally
// the "\" from the cell's top-left to bottom-right) / THIN_D2 (slash=true,
// the "/" from bottom-left to top-right). Unlike FULL/THIN_H/THIN_V, slide_x
// and slide_y here are NOT independent per-axis recedes (that would only
// stay a clean 45-degree line when |slide_x| == |slide_y|, and skew into an
// unstable near-degenerate angle otherwise - visible as jittery, non-smooth
// sliding). Instead slide_x recedes the segment along its own direction
// (from its "a" corner, positive shortens from a's end, negative from b's -
// mirroring how the other kinds treat positive/negative slide) and slide_y
// rigidly shifts the whole segment perpendicular to that direction (0 =
// the true corner-to-corner diagonal) - both preserve the exact angle.
void YR_PERF_ATTR yr__wall_diagonal_endpoints(const YrWall* tile, size_t cell_x, size_t cell_y, bool slash, Vector2 *out_a, Vector2 *out_b) {
    float length_frac = (float)tile->slide_x / 128.0f;
    float depth_frac = (float)tile->slide_y / 128.0f;

    Vector2 a = slash ? (Vector2){(float)cell_x, (float)cell_y + 1.0f} : (Vector2){(float)cell_x, (float)cell_y};
    Vector2 b = slash ? (Vector2){(float)cell_x + 1.0f, (float)cell_y} : (Vector2){(float)cell_x + 1.0f, (float)cell_y + 1.0f};
    Vector2 d = {b.x - a.x, b.y - a.y};
    Vector2 perp = {d.y, -d.x};

    float recede_a = length_frac > 0.0f ? length_frac : 0.0f;
    float recede_b = length_frac < 0.0f ? length_frac : 0.0f;

    out_a->x = a.x + d.x * recede_a + perp.x * depth_frac;
    out_a->y = a.y + d.y * recede_a + perp.y * depth_frac;
    out_b->x = b.x + d.x * recede_b + perp.x * depth_frac;
    out_b->y = b.y + d.y * recede_b + perp.y * depth_frac;
}

// The recede-by-slide box for FULL (its own solid volume) - a positive
// slide recedes the cell's low corner, a negative one the high corner, the
// box never leaves its own cell.
void YR_PERF_ATTR yr__wall_recede_box(const YrWall *tile, size_t cell_x, size_t cell_y, float *x0, float *x1, float *y0, float *y1) {
    float slide_x = (float)tile->slide_x / 128.0f;
    float slide_y = (float)tile->slide_y / 128.0f;
    *x0 = (float)cell_x + (slide_x > 0.0f ? slide_x : 0.0f);
    *x1 = (float)cell_x + 1.0f + (slide_x < 0.0f ? slide_x : 0.0f);
    *y0 = (float)cell_y + (slide_y > 0.0f ? slide_y : 0.0f);
    *y1 = (float)cell_y + 1.0f + (slide_y < 0.0f ? slide_y : 0.0f);
}

// The zero-thickness bar box for THIN_H/THIN_V: slide on the bar's own axis
// clips its length exactly like yr__wall_recede_box's recede, slide on the
// other axis moves its depth within the cell (0 = centered). Only valid for
// tile.kind == YR_WK_THIN_H or YR_WK_THIN_V.
void YR_PERF_ATTR yr__wall_thin_bar_box(const YrWall *tile, size_t cell_x, size_t cell_y, float *x0, float *x1, float *y0, float *y1) {
    float slide_x = (float)tile->slide_x / 128.0f;
    float slide_y = (float)tile->slide_y / 128.0f;
    if (tile->kind == YR_WK_THIN_H) {
        *x0 = (float)cell_x + (slide_x > 0.0f ? slide_x : 0.0f);
        *x1 = (float)cell_x + 1.0f + (slide_x < 0.0f ? slide_x : 0.0f);
        *y0 = *y1 = (float)cell_y + 0.5f * (1.0f + slide_y);
    } else {
        *y0 = (float)cell_y + (slide_y > 0.0f ? slide_y : 0.0f);
        *y1 = (float)cell_y + 1.0f + (slide_y < 0.0f ? slide_y : 0.0f);
        *x0 = *x1 = (float)cell_x + 0.5f * (1.0f + slide_x);
    }
}

static inline float yr_projection_plane_scale(const YrContext *ctx) {
    static float base_scale = 0.0f;
    if (base_scale == 0.0f) base_scale = tanf(YR_FOV_ANGLE / 2.0f);
    return base_scale * (float)ctx->screen_width / (float)ctx->screen_height;
}

static inline Vector2 yr__camera_plane(const YrContext *ctx) {
    float scale = yr_projection_plane_scale(ctx);
    const YrCamera *p = &ctx->camera;
    return (Vector2){.x = -p->dir.y * scale, .y = p->dir.x * scale};
}

static inline void yr_draw_texture_column(
    int x,
    int y,
    int width,
    int height,
    const yr_pixel_t *texture,
    int texture_x,
    int texture_y,
    int texture_height,
    float brightness,
    bool skip_empty) {
    if (width <= 0 || height <= 0 || texture_height <= 0 || !texture) return;
    if (texture_x < 0 || texture_x >= YR_TEXTURE_SIZE) return;

    int scale = (int)((1.0f + brightness) * 256.0f);

    int step = width;
    int tex_pos = (int)(((int64_t)texture_y * (YR_TEXTURE_SIZE << 16)) / texture_height);
    int tex_step = (int)(((int64_t)step * (YR_TEXTURE_SIZE << 16)) / texture_height);

    int row = 0;
    while (row < height) {
        int tex_y = fixed16_to_int(tex_pos);
        tex_pos += tex_step;

        // Merge consecutive rows that sample the same texel (columns taller
        // than the texture) into a single rectangle.
        int run = step;
        while (row + run < height && fixed16_to_int(tex_pos) == tex_y) {
            run += step;
            tex_pos += tex_step;
        }
        if (row + run > height) run = height - row;

        if (tex_y >= 0 && tex_y < YR_TEXTURE_SIZE) {
            yr_pixel_t texel = texture[tex_y * YR_TEXTURE_SIZE + texture_x];
            if (!skip_empty || texel != YR_EMPTY_PIXEL) {
                yr_draw_rectangle(x, y + row, width, run, yr_color_darken(texel, scale));
            }
        }
        row += run;
    }
}

static inline void yr_animate_entity(YrEntity *entity) {
    int tex_id = yr_get_animation_texture(&entity->animation);
    if (tex_id >= 0) entity->texture_id = tex_id;
}

/**
 * Performs raycasting for a single vertical slice of the screen to find wall intersections and render them.
 * Uses DDA algorithm to step through the grid map and find the first wall hit by the ray.
 * It also applies distance-based brightness to the wall slice.
 * @param ctx The current game ctx containing camera, map, and rendering information.
 * @param dir The direction vector of the ray being cast.
 * @param slice_x The x-coordinate of the vertical slice on the screen to render.
 */
void YR_PERF_ATTR yr_raycast_walls(YrContext *ctx, Vector2 dir, int slice_x) {
    YrCamera *p = &ctx->camera;
    float v_shift_base = p->horizon * ctx->screen_height * 0.5f;
    int v_shift = (int)(v_shift_base);
    int z_index = slice_x / ctx->ray_res;

    if (dir.x > -THRESHOLD && dir.x < THRESHOLD) dir.x = (dir.x < 0.0f) ? -THRESHOLD : THRESHOLD;
    if (dir.y > -THRESHOLD && dir.y < THRESHOLD) dir.y = (dir.y < 0.0f) ? -THRESHOLD : THRESHOLD;

    size_t cell_x = (size_t)p->pos.x;
    size_t cell_y = (size_t)p->pos.y;
    float abs_dir_x = dir.x < 0.0f ? -dir.x : dir.x;
    float abs_dir_y = dir.y < 0.0f ? -dir.y : dir.y;
    float delta_dist_x = 1.0f / abs_dir_x;
    float delta_dist_y = 1.0f / abs_dir_y;
    float dist_x; // distance from the ray's origin to the next vertical grid line
    float dist_y; // distance from the ray's origin to the next horizontal grid line
    int step_x;
    int step_y;

    // Determine the step direction and initial distances to the next grid lines based on the ray's direction.
    if (dir.x < 0.0f) {
        // If the ray is pointing left, step_x is -1 and dist_x is the distance to the left grid line.
        step_x = -1;
        dist_x = (p->pos.x - (float)cell_x) * delta_dist_x;
    } else {
        // If the ray is pointing right, step_x is 1 and dist_x is the distance to the right grid line.
        step_x = 1;
        dist_x = ((float)cell_x + 1.0f - p->pos.x) * delta_dist_x;
    }

    if (dir.y < 0.0f) {
        // If the ray is pointing up, step_y is -1 and dist_y is the distance to the upper grid line.
        step_y = -1;
        dist_y = (p->pos.y - (float)cell_y) * delta_dist_y;
    } else {
        // If the ray is pointing down, step_y is 1 and dist_y is the distance to the lower grid line.
        step_y = 1;
        dist_y = ((float)cell_y + 1.0f - p->pos.y) * delta_dist_y;
    }

    float ray_dist = 0.0f;
    bool hit_vertical = false;
    // THIN_H/THIN_V/THIN_D1/THIN_D2/THIN_X can sit inside the camera's own
    // cell (e.g. standing right next to a thin divider), so that starting
    // cell needs testing once before the DDA below starts stepping away
    // from it - otherwise a wall positioned before the first grid boundary
    // would never be found.
    bool first_cell = true;

    while (ray_dist <= YR_MAX_RENDER_DIST) {
        if (first_cell) {
            first_cell = false;
            if (cell_x >= ctx->map.cols || cell_y >= ctx->map.rows) continue;
        } else {
            // calculate the next grid cell the ray will intersect
            if (dist_x < dist_y) {
                cell_x += step_x;
                ray_dist = dist_x;
                dist_x += delta_dist_x;
                hit_vertical = true;
            } else {
                cell_y += step_y;
                ray_dist = dist_y;
                dist_y += delta_dist_y;
                hit_vertical = false;
            }

            if (cell_x >= ctx->map.cols || cell_y >= ctx->map.rows) {
                break;
            }
        }

        YrWall *map_cell = &ctx->map.walls[cell_y * ctx->map.cols + cell_x];
        if (map_cell->kind == YR_WK_EMPTY) continue; // empty cell, keep raycasting

        float wall_dist = ray_dist;
        float diff;
        bool is_diagonal = map_cell->kind == YR_WK_THIN_D1 || map_cell->kind == YR_WK_THIN_D2 || map_cell->kind == YR_WK_THIN_X;

        if (is_diagonal) {
            float best_t = YR_MAX_RENDER_DIST + 1.0f;
            float best_s = 0.0f;
            bool best_side = false;
            bool hit = false;

            if (map_cell->kind == YR_WK_THIN_D1 || map_cell->kind == YR_WK_THIN_X) {
                Vector2 a, b;
                yr__wall_diagonal_endpoints(map_cell, cell_x, cell_y, false, &a, &b);
                float t, s;
                bool side;
                if (yr__thin_diagonal_hit(p->pos, dir, a.x, a.y, b.x, b.y, &t, &s, &side) && t < best_t) {
                    best_t = t;
                    best_s = s;
                    best_side = side;
                    hit = true;
                }
            }
            if (map_cell->kind == YR_WK_THIN_D2 || map_cell->kind == YR_WK_THIN_X) {
                Vector2 a, b;
                yr__wall_diagonal_endpoints(map_cell, cell_x, cell_y, true, &a, &b);
                float t, s;
                bool side;
                if (yr__thin_diagonal_hit(p->pos, dir, a.x, a.y, b.x, b.y, &t, &s, &side) && t < best_t) {
                    best_t = t;
                    best_s = s;
                    best_side = side;
                    hit = true;
                }
            }

            if (!hit) continue; // ray misses both diagonals, keep raycasting

            wall_dist = best_t;
            diff = best_s;
            hit_vertical = best_side;
        } else {
            // FULL is a solid box that recedes by slide/128 along each axis
            // (a positive slide recedes the cell's low corner, a negative
            // one the high corner - the box never leaves its own cell).
            // THIN_H/THIN_V are a zero-thickness divider sitting *inside*
            // the cell, at right angles to their name (THIN_H is a
            // horizontal-*looking* bar that spans the cell's full width and
            // is thin in Y, THIN_V the mirror): slide on the bar's own axis
            // clips its length exactly like FULL's recede, while slide on
            // the other axis moves the bar's depth within the cell (0 =
            // centered, -128/127 = either edge). Either way this needs a
            // real ray-vs-box test rather than assuming the wall is flush
            // with the boundary DDA just found - regular FULL walls with no
            // slide are the only case that already is.
            if (map_cell->kind != YR_WK_FULL || map_cell->slide_x != 0 || map_cell->slide_y != 0) {
                float x0, x1, y0, y1;
                if (map_cell->kind == YR_WK_THIN_H || map_cell->kind == YR_WK_THIN_V) {
                    yr__wall_thin_bar_box(map_cell, cell_x, cell_y, &x0, &x1, &y0, &y1);
                } else {
                    yr__wall_recede_box(map_cell, cell_x, cell_y, &x0, &x1, &y0, &y1);
                }

                float tx1 = (x0 - p->pos.x) / dir.x;
                float tx2 = (x1 - p->pos.x) / dir.x;
                float ty1 = (y0 - p->pos.y) / dir.y;
                float ty2 = (y1 - p->pos.y) / dir.y;

                float tx_min = tx1 < tx2 ? tx1 : tx2;
                float tx_max = tx1 < tx2 ? tx2 : tx1;
                float ty_min = ty1 < ty2 ? ty1 : ty2;
                float ty_max = ty1 < ty2 ? ty2 : ty1;

                float t_enter = tx_min > ty_min ? tx_min : ty_min;
                float t_exit = tx_max < ty_max ? tx_max : ty_max;

                if (t_enter > t_exit || t_exit < 0.0f) continue; // ray misses the wall, keep raycasting

                hit_vertical = tx_min > ty_min;
                wall_dist = t_enter > 0.0f ? t_enter : 0.0f;
            }

            Vector2 rs = {
                .x = p->pos.x + dir.x * wall_dist,
                .y = p->pos.y + dir.y * wall_dist};
            diff = hit_vertical ? rs.y - (float)((int)rs.y) : rs.x - (float)((int)rs.x);
            if (diff < 0.0f) diff += 1.0f;

            float slide_frac = (float)(hit_vertical ? map_cell->slide_y : map_cell->slide_x) / 128.0f;
            diff = slide_frac >= 0.0f ? diff - slide_frac : 1.0f + slide_frac - diff;
        }

        float dist = wall_dist;
        if (dist < THRESHOLD) dist = THRESHOLD;
        if (dist > YR_MAX_RENDER_DIST) break; // stop if the distance exceeds the maximum render distance

        int h = (int)((float)ctx->screen_height / dist);
        float bright_factor = 1.0f / dist - 0.9f;
        if (bright_factor > 0.0f) bright_factor = 0.0f;

        // If the map cell is a special color-coded cell (>= 128), draw a solid color rectangle instead of a texture.
        if (!map_cell->textured) {
            yr_pixel_t c = yr_color_brightness(map_cell->color, bright_factor);
            yr_draw_rectangle(slice_x, (ctx->screen_height - h) / 2 + v_shift, ctx->ray_res, h, c);
            ctx->zbuffer[z_index] = dist; // store distance in z-buffer for sprite rendering
            return;
        }

        // If the map cell corresponds to a texture, calculate the appropriate texture coordinates and draw the textured column.
        const yr_pixel_t *tex = ctx->assets_map[map_cell->texture_id];

        int texture_x = (int)(diff * (float)YR_TEXTURE_SIZE);
        if (texture_x < 0) texture_x = 0;
        if (texture_x >= YR_TEXTURE_SIZE) texture_x = YR_TEXTURE_SIZE - 1;
        if (hit_vertical) texture_x = YR_TEXTURE_SIZE - texture_x - 1;

        int hmax = h;
        if (hmax > ctx->screen_height) hmax = ctx->screen_height;
        int overflow_screen = (h - hmax) / 2;
        int draw_y = (ctx->screen_height - hmax) / 2 + v_shift;

        if (map_cell->transparent) {
            YrRenderSprite s = {
                .obj_type=YR_RO_WALL,
                .texture_id=map_cell->texture_id,
                .dist=dist,
                .texture_x=texture_x,
                .slice_x=slice_x,
            };
            yr__sprites_lock();
            yr_da_append(&ctx->_sprites, s);
            yr__sprites_unlock();
            continue;
        } else {
            ctx->zbuffer[z_index] = dist; // store distance in z-buffer for sprite rendering
            yr_draw_texture_column(
                slice_x,
                draw_y,
                ctx->ray_res,
                hmax,
                tex,
                texture_x,
                overflow_screen,
                h,
                bright_factor,
                false);
        }
        return;
    }

    ctx->zbuffer[z_index] = YR_MAX_RENDER_DIST;
}

/**
 * Renders the walls of the scene by performing raycasting for each vertical slice of the screen.
 * This function iterates over the screen width, casting rays and drawing the corresponding wall slices.
 */
void yr__draw_walls_range(YrContext *ctx, int x_start, int x_end) {
    YrCamera *p = &ctx->camera;
    Vector2 plane = yr__camera_plane(ctx);
    int ray_res = ctx->ray_res;
    float camera_step = 2.0f * (float)ray_res / (float)ctx->screen_width;
    int start_column = x_start / ray_res;
    float camera_x = -1.0f + camera_step * (float)start_column;

    for (int slice_x = x_start; slice_x < x_end; slice_x += ray_res, camera_x += camera_step) {
        Vector2 ray = {
            .x = p->dir.x + plane.x * camera_x,
            .y = p->dir.y + plane.y * camera_x};
        yr_raycast_walls(ctx, ray, slice_x);
    }
}

void yr_draw_walls(YrContext *ctx) {
    yr__draw_walls_range(ctx, 0, ctx->screen_width);
}

/**
 * Renders the background (floor and ceiling) of the scene using a raycasting approach.
 * For each vertical slice of the screen, it calculates the corresponding floor and ceiling texture coordinates and draws the textured columns.
 * It also applies distance-based brightness to create a sense of depth.
 * If no floor or ceiling texture is provided, it fills the respective areas with a solid color (black).
 */
void yr__draw_background_range(YrContext *ctx, int x_start, int x_end) {
    YrCamera *p = &ctx->camera;
    Vector2 plane = yr__camera_plane(ctx);
    Vector2 r0 = {.x = p->dir.x - plane.x, .y = p->dir.y - plane.y};
    Vector2 r1 = {.x = p->dir.x + plane.x, .y = p->dir.y + plane.y};

    Vector2 ray_dir = Vector2Subtract(r1, r0);
    float inv_sw = 1.0f / (float)ctx->screen_width;
    int sh = ctx->screen_height;
    int rr = ctx->ray_res;
    int start_column = x_start / rr;
    float h_cam = (float)sh * 0.5f;
    float half_h = (float)sh * 0.5f;

    const yr_pixel_t *floor_tex = NULL;
    const yr_pixel_t *ceil_tex = NULL;
    if (ctx->map.floor_texture) floor_tex = ctx->assets_map[ctx->map.floor_texture];
    if (ctx->map.ceil_texture) ceil_tex = ctx->assets_map[ctx->map.ceil_texture];

    int hz = (int)(half_h + p->horizon * half_h);
    if (hz < 0) hz = 0;
    if (hz > sh) hz = sh;

    if (ceil_tex) {
        for (int y = 0; y < hz; y += rr) {
            float row_dist = h_cam / (float)(hz - y);
            if (row_dist >= YR_MAX_RENDER_DIST) {
                yr_draw_rectangle(x_start, y, x_end - x_start, rr, YR_BLACK);
                continue;
            }

            float brightness = -(row_dist / YR_MAX_RENDER_DIST);
            int brightness_scale = (int)((1.0f + brightness) * 256.0f);
            float step_x = ray_dir.x * row_dist * (float)rr * inv_sw;
            float step_y = ray_dir.y * row_dist * (float)rr * inv_sw;
            float world_x = p->pos.x + r0.x * row_dist + step_x * (float)start_column;
            float world_y = p->pos.y + r0.y * row_dist + step_y * (float)start_column;

            for (int x = x_start; x < x_end; x += rr, world_x += step_x, world_y += step_y) {
                const yr_pixel_t *tex = ceil_tex;
                if (ctx->map.ceil) {
                    size_t cell_x = (size_t)world_x;
                    size_t cell_y = (size_t)world_y;
                    if (cell_x < ctx->map.cols && cell_y < ctx->map.rows) {
                        uint8_t tex_id = ctx->map.ceil[cell_y * ctx->map.cols + cell_x];
                        if (tex_id) tex = ctx->assets_map[tex_id];
                    }
                }

                int tx = ((int)(world_x * (float)YR_TEXTURE_SIZE)) & (YR_TEXTURE_SIZE - 1);
                int ty = ((int)(world_y * (float)YR_TEXTURE_SIZE)) & (YR_TEXTURE_SIZE - 1);
                yr_pixel_t c = yr_color_darken(tex[ty * YR_TEXTURE_SIZE + tx], brightness_scale);
                yr_draw_rectangle(x, y, rr, rr, c);
            }
        }
    } else if (hz > 0) {
        yr_draw_rectangle(x_start, 0, x_end - x_start, hz, YR_BLACK);
    }

    if (floor_tex) {
        for (int y = hz; y < sh; y += rr) {
            float row_dist = h_cam / (float)(y - hz + 1);
            if (row_dist >= YR_MAX_RENDER_DIST) {
                yr_draw_rectangle(x_start, y, x_end - x_start, rr, YR_BLACK);
                continue;
            }

            float brightness = -(row_dist / YR_MAX_RENDER_DIST);
            int brightness_scale = (int)((1.0f + brightness) * 256.0f);
            float step_x = ray_dir.x * row_dist * (float)rr * inv_sw;
            float step_y = ray_dir.y * row_dist * (float)rr * inv_sw;
            float world_x = p->pos.x + r0.x * row_dist + step_x * (float)start_column;
            float world_y = p->pos.y + r0.y * row_dist + step_y * (float)start_column;

            for (int x = x_start; x < x_end; x += rr, world_x += step_x, world_y += step_y) {
                const yr_pixel_t *tex = floor_tex;
                if (ctx->map.floor) {
                    size_t cell_x = (size_t)world_x;
                    size_t cell_y = (size_t)world_y;
                    if (cell_x < ctx->map.cols && cell_y < ctx->map.rows) {
                        uint8_t tex_id = ctx->map.floor[cell_y * ctx->map.cols + cell_x];
                        if (tex_id) tex = ctx->assets_map[tex_id];
                    }
                }

                int tx = ((int)(world_x * (float)YR_TEXTURE_SIZE)) & (YR_TEXTURE_SIZE - 1);
                int ty = ((int)(world_y * (float)YR_TEXTURE_SIZE)) & (YR_TEXTURE_SIZE - 1);
                yr_pixel_t c = yr_color_darken(tex[ty * YR_TEXTURE_SIZE + tx], brightness_scale);
                yr_draw_rectangle(x, y, rr, rr, c);
            }
        }
    } else if (sh - hz > 0) {
        yr_draw_rectangle(x_start, hz, x_end - x_start, sh - hz, YR_BLACK);
    }
}

void yr_draw_background(YrContext *ctx) {
    yr__draw_background_range(ctx, 0, ctx->screen_width);
}

/**
 * Renders the entities (sprites) in the scene. It first calculates the distance of each entity from the camera, sorts them by distance, and then renders them in back-to-front order to ensure proper occlusion.
 * For each entity, it calculates the appropriate screen position and size based on its distance and renders it using its associated texture.
 * It also applies distance-based brightness to create a sense of depth.
 */
// Computes distances, advances animations and returns the active entities
// sorted farthest-first, ready to be drawn (caller frees the array).
size_t yr__entities_prep(YrContext *ctx) {
    YrCamera *p = &ctx->camera;
    float scale = yr_projection_plane_scale(ctx);
    Vector2 plane = {.x = -p->dir.y * scale, .y = p->dir.x * scale};
    float invDet = 1.0f / (plane.x * p->dir.y - p->dir.x * plane.y);
    // Update entity distances
    for (size_t i = 0; i < ctx->entities.length; i++) {
        YrEntity *e = &ctx->entities.data[i].value;
        if (e->disabled) continue;
        e->dist = Vector2Length(Vector2Subtract(e->pos, p->pos));
        Vector2 rel = Vector2Subtract(e->pos, p->pos);
        YrRenderSprite s = {
            .obj_type=YR_RO_ENTITY,
            .entity=e,
            .dist=(-plane.y * rel.x + plane.x * rel.y) * invDet
        };
        yr_da_append(&ctx->_sprites, s);
        yr_animate_entity(e);
    }

    // Sort entities by distance from the camera in descending order (farthest first) for proper rendering.
    qsort(ctx->_sprites.data, ctx->_sprites.length, sizeof(YrRenderSprite), compare_sprite_dist);
    return ctx->_sprites.length;
}

void yr__update_entities(YrContext *ctx) {
    for (size_t i = 0; i < ctx->entities.length; i++) {
        YrEntity *e = &ctx->entities.data[i].value;
        if (e->disabled || e->update == NULL) continue;
        e->update(ctx, e, ctx->entities.data[i].key);
    }
}

// Draws the prepared sprites, restricted to screen columns [x_start, x_end).
void yr__draw_sprites_range(YrContext *ctx, int x_start, int x_end) {
    YrCamera *p = &ctx->camera;
    float half_screen = ctx->screen_width * 0.5f;
    float projection_scale = (float)ctx->screen_height;
    Vector2 plane = yr__camera_plane(ctx);
    float invDet = 1.0f / (plane.x * p->dir.y - p->dir.x * plane.y);

    for (size_t i = 0; i < ctx->_sprites.length; i++) {
        YrRenderSprite * s = &ctx->_sprites.data[i];
        if (s->obj_type == YR_RO_WALL) {
            if (s->slice_x < x_start || s->slice_x >= x_end) continue;
            int h = (int)((float)ctx->screen_height / s->dist);
            int hmax = h;
            if (hmax > ctx->screen_height) hmax = ctx->screen_height;
            int v_shift = (int)(p->horizon * ctx->screen_height * 0.5f);
            int draw_y = (ctx->screen_height - hmax) / 2 + v_shift;
            int overflow_screen = (h - hmax) / 2;
            float bright_factor = 1.0f / s->dist - 0.9f;
            yr_draw_texture_column(
                s->slice_x,
                draw_y,
                ctx->ray_res,
                hmax,
                ctx->assets_map[s->texture_id],
                s->texture_x,
                overflow_screen,
                h,
                bright_factor,
                true);
        } else if (s->obj_type == YR_RO_ENTITY) {
            YrEntity *e = ctx->_sprites.data[i].entity;
    
            /**
             * Calculate the position of the sprite on the screen using an inverse camera transformation.
             * This involves translating the sprite's world position relative to the camera, applying the inverse of the camera's rotation and projection to determine where it should appear on the screen.
             * The resulting screen coordinates are then used to determine the size and position of the sprite's texture on the screen, as well as its brightness based on distance from the camera.
             */
            Vector2 rel = Vector2Subtract(e->pos, p->pos);
            Vector2 transform = {
                .x = p->dir.y * rel.x - p->dir.x * rel.y,
                .y = -plane.y * rel.x + plane.x * rel.y};
            transform = Vector2Scale(transform, invDet);
    
            if (transform.y <= 0.0f || transform.y >= YR_MAX_RENDER_DIST) continue;
    
            int spriteScreenX = (int)(half_screen * (1 + transform.x / transform.y));
            int v_shift = (int)(p->horizon * ctx->screen_height * 0.5f);
            int vmove = (int)((e->vmove * projection_scale) / transform.y);
    
            int spriteHeight = abs((int)((projection_scale * (1.0 - e->vdiv)) / transform.y));
            if (spriteHeight <= 0) continue;
            int spriteTop = (ctx->screen_height - spriteHeight) / 2 + vmove + v_shift;
            int spriteBottom = spriteTop + spriteHeight;
            int drawStartY = spriteTop;
            if (drawStartY < 0) drawStartY = 0;
            int drawEndY = spriteBottom;
            if (drawEndY > ctx->screen_height) drawEndY = ctx->screen_height;
            if (drawEndY <= drawStartY) continue;
    
            int spriteWidth = abs((int)((projection_scale * (1.0 - e->hdiv)) / transform.y));
            if (spriteWidth <= 0) continue;
            int spriteLeft = spriteScreenX - spriteWidth / 2;
            int spriteRight = spriteLeft + spriteWidth;
            int drawStartX = spriteLeft;
            if (drawStartX < x_start) drawStartX = x_start;
            int drawEndX = spriteRight;
            if (drawEndX > x_end) drawEndX = x_end;
            if (drawEndX <= drawStartX) continue;
    
            const yr_pixel_t *tex = ctx->assets_map[e->texture_id];
    
            // Render the sprite column by column, applying distance-based brightness and checking against the z-buffer for proper occlusion with walls.
            for (int x = drawStartX; x < drawEndX; x += ctx->ray_res) {
                int texX = (x - spriteLeft) * YR_TEXTURE_SIZE / spriteWidth;
                if (texX < 0) texX = 0;
                if (texX >= YR_TEXTURE_SIZE) texX = YR_TEXTURE_SIZE - 1;
    
                if (transform.y < ctx->zbuffer[x / ctx->ray_res]) {
                    int texture_y = drawStartY - spriteTop;
                    yr_draw_texture_column(
                        x,
                        drawStartY,
                        ctx->ray_res,
                        drawEndY - drawStartY,
                        tex,
                        texX,
                        texture_y,
                        spriteHeight,
                        -(transform.y / YR_MAX_RENDER_DIST),
                        true);
                }
            }
        }
    }
}

void yr_draw_entities(YrContext *ctx) {
    yr__entities_prep(ctx);
    yr__draw_sprites_range(ctx, 0, ctx->screen_width);
    yr_da_shrink(&ctx->_sprites);
    ctx->_sprites.length=0;
    yr__update_entities(ctx);
}

YrContext yr_context = {0};

void yr__init_game() {
    yr_context.screen_width = YR_LCD_W;
    yr_context.screen_height = YR_LCD_H;
    yr_context.game_title = "Yari";
    yr_context.target_fps = 0;
    yr_context.ray_res = 1;
    yr_context.next_entity_id = 0;
    yr_init_game(&yr_context);
    #ifdef ESP32
    yr_context.screen_width = YR_LCD_W;
    yr_context.screen_height = YR_LCD_H;
    #endif
    if (yr_context.ray_res == 0) yr_context.ray_res = 1;
    yr_context.camera.dir = Vector2Normalize(yr_context.camera.dir);
    yr_context.zbuffer = malloc(sizeof(float) * ((yr_context.screen_width + yr_context.ray_res - 1) / yr_context.ray_res));
    yr_renderer_init(
        yr_context.screen_width,
        yr_context.screen_height,
        yr_context.game_title,
        yr_context.target_fps);
    yr_inputs_init();
}

void yr_draw_game(YrContext *ctx) {
#ifdef YR_MULTITHREAD
    yr__draw_game_multithread(ctx);
#else // !YR_MULTITHREAD
    yr_draw_background(ctx);
    yr_draw_walls(ctx);
    yr_draw_entities(ctx);
#endif // YR_MULTITHREAD
}

void yr__update_game() {
    yr_begin_drawing();
    yr_update_game(&yr_context);
    yr_render_screen();
    yr_end_drawing();
}

void yr__free_game() {
    free(yr_context.zbuffer);
    yr_hm_free(&yr_context.entities);
    yr_da_free(&yr_context._sprites);
}

size_t yr_create_entity_ex(YrContext *ctx, YrEntity e, void *data) {
    if (e.init) e.init(&e, data);
    yr_hm_set(&ctx->entities, ctx->next_entity_id, e);
    return ctx->next_entity_id++;
}

void yr_remove_entity(YrContext *ctx, size_t id) {
    YrEntity *e = yr_hm_try(&ctx->entities, id);
    if (!e) return;

    yr_da_free(&e->animation);
    if (e->cleanup) e->cleanup(e);
    yr_hm_remove(&ctx->entities, id);
}

void yr_clear_entities(YrContext *ctx) {
    yr_foreach(&ctx->entities, kv) {
        if (kv->value.cleanup) kv->value.cleanup(&kv->value);
        yr_da_free(&kv->value.animation);
    }
    yr_hm_free(&ctx->entities);
}

size_t yr_get_entity_id(YrEntity *e) {
    return *((size_t *)(((uint8_t *)(e)) - sizeof(size_t)));
}
