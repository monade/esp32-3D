#include "yari.h"

#include <stdlib.h>

#ifdef ESP32_MULTITHREAD
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

// #define YR_PROFILE 1
#if YR_PROFILE
#include <stdio.h>
#include <string.h>
static struct {
    float bg, walls, ents, game, tx, last;
    int frames;
} yr_prof;
#endif

#define THRESHOLD 0.0001

static int compare_sprite_dist(const void *a, const void *b) {
    const YrEntity *sa = *(const YrEntity **)a;
    const YrEntity *sb = *(const YrEntity **)b;
    if (sa->dist < sb->dist) return 1;
    if (sa->dist > sb->dist) return -1;
    return 0;
}

static inline int fixed16_to_int(int value) {
    return value >= 0 ? (value >> 16) : -((-value) >> 16);
}

static inline float yr_projection_plane_scale(const YrGameState *state) {
    static float base_scale = 0.0f;
    if (base_scale == 0.0f) base_scale = tanf(YR_FOV_ANGLE / 2.0f);
    return base_scale * (float)state->screen_width / (float)state->screen_height;
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
    bool skip_empty
) {
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
 * @param state The current game state containing camera, map, and rendering information.
 * @param dir The direction vector of the ray being cast.
 * @param slice_x The x-coordinate of the vertical slice on the screen to render.
 */
void yr_raycast_walls(YrGameState *state, Vector2 dir, int slice_x) {
    YrCamera *p = &state->camera;
    float v_shift_base = p->horizon * state->screen_height * 0.5f;
    float roll = ((float)slice_x - state->screen_width * 0.5f) * tanf(p->angle);
    int v_shift = (int)(v_shift_base + roll);
    int z_index = slice_x / state->ray_res;

    if (dir.x > -THRESHOLD && dir.x < THRESHOLD) dir.x = (dir.x < 0.0f) ? -THRESHOLD : THRESHOLD;
    if (dir.y > -THRESHOLD && dir.y < THRESHOLD) dir.y = (dir.y < 0.0f) ? -THRESHOLD : THRESHOLD;

    int cell_x = (int)p->pos.x;
    int cell_y = (int)p->pos.y;
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

    while (ray_dist <= YR_MAX_RENDER_DIST) {
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

        if (cell_x < 0 || cell_x >= state->map_cols || cell_y < 0 || cell_y >= state->map_rows) {
            break;
        }

        uint8_t map_cell = state->map[cell_y * state->map_cols + cell_x];
        if (!map_cell) continue; // empty cell, keep raycasting

        float dist = ray_dist;
        if (dist < THRESHOLD) dist = THRESHOLD;
        if (dist > YR_MAX_RENDER_DIST) break; // stop if the distance exceeds the maximum render distance

        state->zbuffer[z_index] = dist; // store distance in z-buffer for sprite rendering
        int h = (int)((float)state->screen_height / dist);
        float bright_factor = 1.0f / dist - 0.9f;
        if (bright_factor > 0.0f) bright_factor = 0.0f;

        // If the map cell is a special color-coded cell (>= 128), draw a solid color rectangle instead of a texture.
        if (map_cell >= 128) {
            yr_pixel_t c = yr_color_brightness(yr_color_map[map_cell - 128], bright_factor);
            yr_draw_rectangle(slice_x, (state->screen_height - h) / 2 + v_shift, state->ray_res, h, c);
            return;
        }

        // If the map cell corresponds to a texture, calculate the appropriate texture coordinates and draw the textured column.
        const yr_pixel_t *tex = state->assets_map[map_cell];
        Vector2 rs = {
            .x = p->pos.x + dir.x * ray_dist,
            .y = p->pos.y + dir.y * ray_dist
        };
        float diff = hit_vertical ? rs.y - (float)((int)rs.y) : rs.x - (float)((int)rs.x);
        if (diff < 0.0f) diff += 1.0f;

        int texture_x = (int)(diff * (float)YR_TEXTURE_SIZE);
        if (texture_x < 0) texture_x = 0;
        if (texture_x >= YR_TEXTURE_SIZE) texture_x = YR_TEXTURE_SIZE - 1;
        if (hit_vertical) texture_x = YR_TEXTURE_SIZE - texture_x - 1;

        int hmax = h;
        if (hmax > state->screen_height) hmax = state->screen_height;
        int overflow_screen = (h - hmax) / 2;
        int draw_y = (state->screen_height - hmax) / 2 + v_shift;
        yr_draw_texture_column(
            slice_x,
            draw_y,
            state->ray_res,
            hmax,
            tex,
            texture_x,
            overflow_screen,
            h,
            bright_factor,
            false);
        return;
    }

    state->zbuffer[z_index] = YR_MAX_RENDER_DIST;
}

/**
 * Renders the walls of the scene by performing raycasting for each vertical slice of the screen.
 * This function iterates over the screen width, casting rays and drawing the corresponding wall slices.
 */
static void yr_draw_walls_range(YrGameState *state, int x_start, int x_end) {
    YrCamera *p = &state->camera;
    float scale = yr_projection_plane_scale(state);
    Vector2 plane = { .x = -p->dir.y * scale, .y = p->dir.x * scale };
    int ray_res = state->ray_res;
    float camera_step = 2.0f * (float)ray_res / (float)state->screen_width;
    int start_column = x_start / ray_res;
    float camera_x = -1.0f + camera_step * (float)start_column;

    for (int slice_x = x_start; slice_x < x_end; slice_x += ray_res, camera_x += camera_step) {
        Vector2 ray = {
            .x = p->dir.x + plane.x * camera_x,
            .y = p->dir.y + plane.y * camera_x
        };
        yr_raycast_walls(state, ray, slice_x);
    }
}

void yr_draw_walls(YrGameState *state) {
    yr_draw_walls_range(state, 0, state->screen_width);
}


/**
 * Renders the background (floor and ceiling) of the scene using a raycasting approach.
 * For each vertical slice of the screen, it calculates the corresponding floor and ceiling texture coordinates and draws the textured columns. 
 * It also applies distance-based brightness to create a sense of depth.
 * If no floor or ceiling texture is provided, it fills the respective areas with a solid color (black).
 */
static void yr_draw_background_range(YrGameState *state, int x_start, int x_end) {
    YrCamera *p = &state->camera;
    float scale = yr_projection_plane_scale(state);
    Vector2 plane = { .x = -p->dir.y * scale, .y = p->dir.x * scale };
    Vector2 r0 = { .x = p->dir.x - plane.x, .y = p->dir.y - plane.y };
    Vector2 r1 = { .x = p->dir.x + plane.x, .y = p->dir.y + plane.y };

    Vector2 ray_dir = Vector2Subtract(r1, r0);
    float inv_sw = 1.0f / (float)state->screen_width;
    int sh = state->screen_height;
    int rr = state->ray_res;
    int start_column = x_start / rr;
    float h_cam = (float)sh * 0.5f;
    float half_h = (float)sh * 0.5f;

    const yr_pixel_t *floor_tex = NULL;
    const yr_pixel_t *ceil_tex = NULL;
    if (state->floor_texture) floor_tex = state->assets_map[state->floor_texture];
    if (state->ceil_texture) ceil_tex = state->assets_map[state->ceil_texture];

    if (p->angle == 0.0f) {
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
                    if (state->map_ceil) {
                        int cell_x = (int)world_x;
                        int cell_y = (int)world_y;
                        if (cell_x >= 0 && cell_x < state->map_cols && cell_y >= 0 && cell_y < state->map_rows) {
                            uint8_t tex_id = state->map_ceil[cell_y * state->map_cols + cell_x];
                            if (tex_id) tex = state->assets_map[tex_id];
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
                    if (state->map_floor) {
                        int cell_x = (int)world_x;
                        int cell_y = (int)world_y;
                        if (cell_x >= 0 && cell_x < state->map_cols && cell_y >= 0 && cell_y < state->map_rows) {
                            uint8_t tex_id = state->map_floor[cell_y * state->map_cols + cell_x];
                            if (tex_id) tex = state->assets_map[tex_id];
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

        return;
    }

    float tan_angle = tanf(p->angle);
    float half_w = (float)state->screen_width * 0.5f;

    for (int x = x_start; x < x_end; x += rr) {
        float horizon = half_h + p->horizon * half_h + ((float)x - half_w) * tan_angle;
        int hz = (int)horizon;
        if (hz < 0) hz = 0;
        if (hz > sh) hz = sh;

        // Calculate the base ray direction for the current vertical slice
        Vector2 base_ray = Vector2Add(r0, Vector2Scale(ray_dir, (float)x * inv_sw));

        if (ceil_tex) {
            for (int y = 0; y < hz; y += rr) {
                float row_dist = h_cam / (float)(hz - y); // distance from the camera to the point on the ceiling corresponding to this pixel row
                if (row_dist >= YR_MAX_RENDER_DIST) {
                    yr_draw_rectangle(x, y, rr, rr, YR_BLACK);
                    continue;
                }
                float brightness = -(row_dist / YR_MAX_RENDER_DIST);
                Vector2 w = Vector2Add(p->pos, Vector2Scale(base_ray, row_dist)); // world coordinates of the point on the ceiling corresponding to this pixel row
                if (state->map_ceil) {
                    int cell_x = (int)w.x;
                    int cell_y = (int)w.y;
                    if(cell_x >= 0 && cell_x < state->map_cols && cell_y >= 0 && cell_y < state->map_rows && state->map_ceil[cell_y * state->map_cols + cell_x]) {
                        ceil_tex = state->assets_map[state->map_ceil[cell_y * state->map_cols + cell_x]];
                    } else {
                        ceil_tex = state->assets_map[state->ceil_texture];
                    }
                }
                int tx = ((int)(w.x * (float)YR_TEXTURE_SIZE)) & (YR_TEXTURE_SIZE - 1);
                int ty = ((int)(w.y * (float)YR_TEXTURE_SIZE)) & (YR_TEXTURE_SIZE - 1);
                yr_pixel_t c = yr_color_brightness(ceil_tex[ty * YR_TEXTURE_SIZE + tx], brightness);
                yr_draw_rectangle(x, y, rr, rr, c);
            }
        } else if (hz > 0) {
            yr_draw_rectangle(x, 0, rr, hz, YR_BLACK);
        }

        if (floor_tex) {
            for (int y = hz; y < sh; y += rr) {
                float row_dist = h_cam / (float)(y - hz + 1); // distance from the camera to the point on the floor corresponding to this pixel row
                if (row_dist >= YR_MAX_RENDER_DIST) {
                    yr_draw_rectangle(x, y, rr, rr, YR_BLACK);
                    continue;
                }
                float brightness = -(row_dist / YR_MAX_RENDER_DIST);
                Vector2 w = Vector2Add(p->pos, Vector2Scale(base_ray, row_dist)); // world coordinates of the point on the floor corresponding to this pixel row
                if (state->map_floor && w.x >= 0 && (int)w.x < state->map_cols && w.y >= 0 && (int)w.y < state->map_rows) {
                    int cell_x = (int)w.x;
                    int cell_y = (int)w.y;
                    if(cell_x >= 0 && cell_x < state->map_cols && cell_y >= 0 && cell_y < state->map_rows && state->map_floor[cell_y * state->map_cols + cell_x]) {
                        floor_tex = state->assets_map[state->map_floor[cell_y * state->map_cols + cell_x]];
                    } else {
                        floor_tex = state->assets_map[state->floor_texture];
                    }
                }
                int tx = ((int)(w.x * (float)YR_TEXTURE_SIZE)) & (YR_TEXTURE_SIZE - 1);
                int ty = ((int)(w.y * (float)YR_TEXTURE_SIZE)) & (YR_TEXTURE_SIZE - 1);
                yr_pixel_t c = yr_color_brightness(floor_tex[ty * YR_TEXTURE_SIZE + tx], brightness);
                yr_draw_rectangle(x, y, rr, rr, c);
            }
        } else if (sh - hz > 0) {
            yr_draw_rectangle(x, hz, rr, sh - hz, YR_BLACK);
        }
    }
}

void yr_draw_background(YrGameState *state) {
    yr_draw_background_range(state, 0, state->screen_width);
}

/**
 * Renders the entities (sprites) in the scene. It first calculates the distance of each entity from the camera, sorts them by distance, and then renders them in back-to-front order to ensure proper occlusion. 
 * For each entity, it calculates the appropriate screen position and size based on its distance and renders it using its associated texture. 
 * It also applies distance-based brightness to create a sense of depth.
 */
// Computes distances, advances animations and returns the active entities
// sorted farthest-first, ready to be drawn (caller frees the array).
static size_t yr_entities_prep(YrGameState *state, YrEntity ***out_entities) {
    YrCamera *p = &state->camera;
    YrEntity** entities = malloc(sizeof(*entities)*state->entities.length);
    size_t active_entities_count = 0;
    // Update entity distances
    for (size_t i = 0; i < state->entities.length; i++) {
        YrEntity *e = &state->entities.data[i].value;
        if (e->disabled) continue;
        e->dist = Vector2Length(Vector2Subtract(e->pos, p->pos));
        entities[active_entities_count++] = e;
        yr_animate_entity(e);
    }

    // Sort entities by distance from the camera in descending order (farthest first) for proper rendering.
    qsort(entities, active_entities_count, sizeof(YrEntity*), compare_sprite_dist);
    *out_entities = entities;
    return active_entities_count;
}

static void yr_update_entities(YrGameState *state) {
    for (size_t i = 0; i < state->entities.length; i++) {
        YrEntity *e = &state->entities.data[i].value;
        if (e->disabled || e->update == NULL) continue;
        e->update(state, e, state->entities.data[i].key);
    }
}

// Draws the prepared sprites, restricted to screen columns [x_start, x_end).
static void yr_draw_sprites_range(
    YrGameState *state,
    YrEntity **entities,
    size_t active_entities_count,
    int x_start,
    int x_end
) {
    YrCamera *p = &state->camera;
    float half_screen = state->screen_width * 0.5f;
    float tan_angle = tanf(p->angle);
    float scale = yr_projection_plane_scale(state);
    float projection_scale = (float)state->screen_height;
    Vector2 plane = { .x = -p->dir.y * scale, .y = p->dir.x * scale };
    float invDet = 1.0f / (plane.x * p->dir.y - p->dir.x * plane.y);

    for (size_t i = 0; i < active_entities_count; i++) {
        // if (entities[i].disabled) continue;
        YrEntity *e = entities[i];

        /**
         * Calculate the position of the sprite on the screen using an inverse camera transformation.
         * This involves translating the sprite's world position relative to the camera, applying the inverse of the camera's rotation and projection to determine where it should appear on the screen.
         * The resulting screen coordinates are then used to determine the size and position of the sprite's texture on the screen, as well as its brightness based on distance from the camera.
         */
        Vector2 rel = Vector2Subtract(e->pos, p->pos);
        Vector2 transform = {
            .x = p->dir.y * rel.x - p->dir.x * rel.y,
            .y = -plane.y * rel.x + plane.x * rel.y
        };
        transform = Vector2Scale(transform, invDet);

        if (transform.y <= 0.0f || transform.y >= YR_MAX_RENDER_DIST) continue;

        int spriteScreenX = (int)(half_screen * (1 + transform.x / transform.y));
        int v_shift = (int)(p->horizon * state->screen_height * 0.5f + ((float)spriteScreenX - half_screen) * tan_angle);
        int vmove = (int)((e->vmove * projection_scale) / transform.y);

        int spriteHeight = abs((int)((projection_scale * (1.0 - e->vdiv)) / transform.y));
        if (spriteHeight <= 0) continue;
        int spriteTop = (state->screen_height - spriteHeight) / 2 + vmove + v_shift;
        int spriteBottom = spriteTop + spriteHeight;
        int drawStartY = spriteTop;
        if (drawStartY < 0) drawStartY = 0;
        int drawEndY = spriteBottom;
        if (drawEndY > state->screen_height) drawEndY = state->screen_height;
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

        const yr_pixel_t *tex = state->assets_map[e->texture_id];

        // Render the sprite column by column, applying distance-based brightness and checking against the z-buffer for proper occlusion with walls.
        for (int x = drawStartX; x < drawEndX; x += state->ray_res) {
            int texX = (x - spriteLeft) * YR_TEXTURE_SIZE / spriteWidth;
            if (texX < 0) texX = 0;
            if (texX >= YR_TEXTURE_SIZE) texX = YR_TEXTURE_SIZE - 1;

            if (transform.y < state->zbuffer[x / state->ray_res]) {
                int texture_y = drawStartY - spriteTop;
                yr_draw_texture_column(
                    x,
                    drawStartY,
                    state->ray_res,
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

void yr_draw_entities(YrGameState *state) {
    YrEntity **entities = NULL;
    size_t active_entities_count = yr_entities_prep(state, &entities);
    yr_draw_sprites_range(state, entities, active_entities_count, 0, state->screen_width);
    free(entities);
    yr_update_entities(state);
}

YrGameState state = {0};

#ifdef ESP32_MULTITHREAD
// Dual-core work splitting: a worker task pinned to the other core runs the
// first half of a job while the main core runs the second half. Used for the
// screen rendering (split by columns) and yr_apply_color_filter (split by
// rows). All shared state must be read-only during the parallel phase and
// each half must only write data disjoint from the other.
static TaskHandle_t yr_render_worker_handle = NULL;
static TaskHandle_t yr_render_main_handle = NULL;
static bool yr_render_worker_failed = false;
static void (*yr_par_job)(void *ctx, int start, int end) = NULL;
static void *yr_par_ctx = NULL;
static int yr_par_mid = 0;
static YrEntity **yr_par_entities = NULL;
static size_t yr_par_entity_count = 0;

static void yr_render_worker(void *arg) {
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        yr_par_job(yr_par_ctx, 0, yr_par_mid);
        xTaskNotifyGive(yr_render_main_handle);
    }
}

static bool yr_render_worker_start(void) {
    if (yr_render_worker_handle) return true;
    if (yr_render_worker_failed) return false;

    yr_render_main_handle = xTaskGetCurrentTaskHandle();

    // The main task runs on core 0 (ESP_MAIN_TASK_AFFINITY_CPU0); pin the
    // render worker to core 1. On unicore configs this fails and we fall
    // back to single-core execution.
    if (xTaskCreatePinnedToCore(yr_render_worker, "yr_render", 4096, NULL,
                                uxTaskPriorityGet(NULL), &yr_render_worker_handle, 1) != pdPASS) {
        yr_render_worker_handle = NULL;
        yr_render_worker_failed = true;
        return false;
    }
    return true;
}

// Runs job over [0, total): the worker executes [0, total/2) on core 1 while
// the caller executes [total/2, total), then they join. Falls back to one
// sequential call if the worker can't start. Call only from the main task,
// never from inside another split job.
void yr_run_split(void (*job)(void *ctx, int start, int end), void *ctx, int total) {
    if (total > 1 && yr_render_worker_start()) {
        yr_par_job = job;
        yr_par_ctx = ctx;
        yr_par_mid = total / 2;
        xTaskNotifyGive(yr_render_worker_handle);
        job(ctx, yr_par_mid, total);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    } else {
        job(ctx, 0, total);
    }
}

// Draws background, walls and sprites for a range of screen columns. Units
// are ray_res-wide columns; the last unit absorbs any screen-width remainder.
static void yr_draw_half_job(void *ctx, int start, int end) {
    (void)ctx;
    int rr = state.ray_res;
    int total = state.screen_width / rr;
    int x0 = start * rr;
    int x1 = (end == total) ? state.screen_width : end * rr;
    yr_draw_background_range(&state, x0, x1);
    yr_draw_walls_range(&state, x0, x1);
    yr_draw_sprites_range(&state, yr_par_entities, yr_par_entity_count, x0, x1);
}
#endif // ESP32_MULTITHREAD

void yr__init_game() {
    state.screen_width = YR_LCD_W;
    state.screen_height = YR_LCD_H;
    state.game_title = "Yari";
    state.target_fps = 0;
    state.ray_res = 1;
    state.next_entity_id = 0;
    yr_init_game(&state);
    if (state.ray_res == 0) state.ray_res = 1;
    state.camera.dir = Vector2Normalize(state.camera.dir);
    state.zbuffer = malloc(sizeof(float) * ((state.screen_width + state.ray_res - 1) / state.ray_res));
    yr_renderer_init(
        state.screen_width,
        state.screen_height,
        state.game_title,
        state.target_fps
        );
    yr_inputs_init();
}

void yr_draw_game() {
#ifdef ESP32_MULTITHREAD
#if YR_PROFILE
  float t0 = yr_get_time();
#endif
  YrEntity **entities = NULL;
  size_t entity_count = yr_entities_prep(&state, &entities);

  yr_par_entities = entities;
  yr_par_entity_count = entity_count;
  yr_run_split(yr_draw_half_job, NULL, state.screen_width / state.ray_res);

  free(entities);
  yr_update_entities(&state);
#if YR_PROFILE
  // Parallel mode: the whole draw (prep + both halves + updates) lands in
  // "ents"; bg/walls stay at 0. Watch fps and ents for comparisons.
  yr_prof.ents += yr_get_time() - t0;
#endif
#else // !ESP32_MULTITHREAD
#if YR_PROFILE
  float t0 = yr_get_time();
  yr_draw_background(&state);
  float t1 = yr_get_time();
  yr_draw_walls(&state);
  float t2 = yr_get_time();
  yr_draw_entities(&state);
  float t3 = yr_get_time();
  yr_prof.bg += t1 - t0;
  yr_prof.walls += t2 - t1;
  yr_prof.ents += t3 - t2;
#else
  yr_draw_background(&state);
  yr_draw_walls(&state);
  yr_draw_entities(&state);
#endif
#endif // ESP32_MULTITHREAD
}


void yr__update_game() {
#if YR_PROFILE
  float f0 = yr_get_time();
  yr_begin_drawing();
  yr_update_game(&state);
  float f1 = yr_get_time();
  yr_render_screen();
  float f2 = yr_get_time();
  yr_prof.game += f1 - f0;
  yr_prof.tx += f2 - f1;

  if (++yr_prof.frames >= 60) {
    float n = (float)yr_prof.frames;
    float fps = (yr_prof.last > 0.0f) ? n / (f2 - yr_prof.last) : 0.0f;
    // "logic" = everything in yr_update_game outside the three draw phases
    // (game logic, HUD, input handling).
    float logic = yr_prof.game - yr_prof.bg - yr_prof.walls - yr_prof.ents;
    printf("[prof] fps=%.1f | bg=%.2f walls=%.2f ents=%.2f logic=%.2f tx=%.2f ms\n",
           fps,
           1000.0f * yr_prof.bg / n,
           1000.0f * yr_prof.walls / n,
           1000.0f * yr_prof.ents / n,
           1000.0f * logic / n,
           1000.0f * yr_prof.tx / n);
    memset(&yr_prof, 0, sizeof(yr_prof));
    yr_prof.last = f2;
  }
#else
  yr_begin_drawing();
  yr_update_game(&state);
  yr_render_screen();
#endif
  yr_end_drawing();
}

void yr__free_game() {
    free(state.zbuffer);
    yr_da_free(&state.entities);
}

size_t yr_create_entity_ex(YrGameState *state, YrEntity e, void *data) {
    if(e.init) e.init(&e, data);
    yr_hm_set(&state->entities, state->next_entity_id, e);
    return state->next_entity_id++;
}

void yr_remove_entity(YrGameState *state, size_t id) {
    YrEntity *e = yr_hm_try(&state->entities, id);
    if(!e) return;
    
    if(e->cleanup) e->cleanup(e);
    yr_da_free(&e->animation);
    yr_hm_remove(&state->entities, id);
}

size_t yr_get_entity_id(YrEntity *e) {
    return *((size_t*)(((uint8_t*)(e)) - sizeof(size_t)));
}


