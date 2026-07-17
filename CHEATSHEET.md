# YARI Cheatsheet

Quick reference for every function exported by the `yari` engine (`src/yari/`).

All engine symbols are prefixed `yr_`/`Yr`. If you `#define YARI_NO_PREFIX`
before `#include <yari.h>`, an unprefixed alias is generated for every
function, macro and type below (e.g. `yr_draw_rectangle` → `draw_rectangle`,
`YrContext` → `Context`). Math functions from `raymath.h` (`Vector2Add`,
`MatrixMultiply`, ...) are never prefixed, matching raylib.

## Table of Contents

- [Core Types](#core-types)
- [Configuration Macros](#configuration-macros)
- [Game Loop](#game-loop-yarih)
- [Renderer - Drawing](#renderer--drawing-rendererh)
- [Renderer - Screen & Lifecycle](#renderer--screen--lifecycle-rendererh)
- [Colors](#colors-colorsh)
- [Physics & Collisions](#physics--collisions-physicsh)
- [Input](#input-inputsh)
- [Utils - Timers](#utils--timers-yari_utilsh)
- [Utils - Sprite Animation](#utils--sprite-animation-yari_utilsh)
- [Utils - Entities](#utils--entities-yarih)
- [Dynamic Array](#dynamic-array-dah)
- [Hash Map & Hash Set](#hash-map--hash-set-hth)
- [Math - raymath](#math--raymath-raymathh)

---

## Core Types

```c
YrContext     // central engine state: camera, map, entities, screen, assets (see README - `Engine` context)
YrCamera        // { Vector2 pos; Vector2 dir; float horizon; float angle; }
YrEntity        // a sprite/object: pos, texture_id, kind, embedded animation stack, collision info, init/update/cleanup callbacks (see README - `Entities`)
YrEntityMap     // yr_Hm(size_t, YrEntity) - hash map of entity id -> YrEntity, backs ctx->entities
yr_pixel_t      // framebuffer pixel: uint16_t (RGB565, ESP32/COLOR_565) or uint32_t (ARGB, desktop/web)
yr_font_t       // baked bitmap font: 1-bit atlas + yr_glyph_t[96] (ASCII 32-127)
yr_glyph_t      // one glyph's atlas bounds, offset and advance (mirrors stbtt_bakedchar)

YrCollisionInfo // result of a collision/ray query: type + wall cell or entity pointer

YrTimer         // { float ends_at; unsigned int count; } lightweight countdown timer
YrAnimation     // animation config: frame list, frame count, per-frame duration (passed to yr_start_animation)
YrAnimationStack// dynamic array (stack) of YrAnimationData; drives entity->texture_id automatically for entities, or read manually via yr_get_animation_texture
```

## Configuration Macros

```c
YR_MAX_RENDER_DIST     // max raycast distance in map units (default 20.0, overridable)
YR_TEXTURE_SIZE        // wall/floor/ceiling/sprite texture size in pixels (default 64, overridable)
YR_CMSK_NONE           // collision mask: matches nothing (0)
YR_CMSK_WALL           // collision mask: walls only (1)
YR_CMSK_ALL            // collision mask: everything (-1)
```

---

## Game Loop (`yari.h`)

```c
void yr_init_game(YrContext *ctx);                          // Implement in game code: configure initial state, called once at startup
void yr_update_game(YrContext *ctx);                        // Implement in game code: update state and draw, called every frame

void yr_draw_game(void);                                        // Draws background, walls and entities using the global engine state

// low level drawing functions
void yr_draw_background(YrContext *ctx);                    // Draws the floor and ceiling (textured, per-cell, or solid black)
void yr_draw_walls(YrContext *ctx);                         // Raycasts and draws every wall column for the frame
void yr_draw_entities(YrContext *ctx);                      // Sorts entities back-to-front, draws sprites, then runs their update callbacks
```

## Renderer - Drawing (`renderer.h`)

```c
void yr_clear_screen(yr_pixel_t color);                                       // Fills the entire framebuffer with a color
void yr_draw_pixel(int x, int y, yr_pixel_t color);                           // Draws a single pixel (macro over yr_draw_rectangle)
void yr_draw_rectangle(int x, int y, int width, int height, yr_pixel_t color); // Draws a filled rectangle
void yr_draw_rectangle_line(int x, int y, int width, int height, int thickness, yr_pixel_t color); // Draws a rectangle outline
void yr_draw_line(int x0, int y0, int x1, int y1, int thickness, yr_pixel_t color); // Draws a line between two points
void yr_draw_circle(int x, int y, int radius, yr_pixel_t color);              // Draws a filled circle
void yr_draw_circle_line(int x, int y, int radius, int thickness, yr_pixel_t color); // Draws a circle outline
void yr_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, yr_pixel_t color); // Draws a filled triangle
void yr_draw_triangle_line(int x0, int y0, int x1, int y1, int x2, int y2, int thickness, yr_pixel_t color); // Draws a triangle outline
void yr_draw_texture(int x, int y, int width, int height, const yr_pixel_t *texture, int texture_width, int texture_height, bool skip_empty); // Draws a scaled 2D texture (HUD/UI elements)
void yr_draw_text(const char *text, int x, int y, const yr_font_t *font, yr_pixel_t c); // Draws bitmap text using a baked font (YR_FONT_SM/MD/LG/XL)
```

Color filters (full-screen post-processing, applied once per pixel regardless
of overdraw - see README - `Color filters`):

```c
typedef void (*YrColorFilterCallback)(int x, int y, yr_pixel_t *color, void *user_data);

void yr_apply_color_filter(YrColorFilterCallback apply, void *user_data); // Runs `apply` once over every pixel of the framebuffer
```

## Renderer - Screen & Lifecycle (`renderer.h`)

```c
float yr_get_frame_time(void);                                   // Frame delta time in seconds
float yr_get_time(void);                                         // Platform time in seconds since start
float yr_get_fps(void);                                          // Current FPS estimate

// low level frame buffer, care the format is different on every rendering backend
yr_pixel_t *get_framebuffer(void);                                // Returns the raw framebuffer pointer
```

## Colors (`colors.h`)

```c
yr_pixel_t yr_color_darken(yr_pixel_t color, int scale);         // Darkens a color; scale 0..256 (0 = black, 256 = unchanged)
yr_pixel_t yr_color_brightness(yr_pixel_t color, float factor);  // Brightens (factor > 0) or darkens (factor < 0) a color, factor in -1..1
```

Named color constants (`YR_BLACK`, `YR_WHITE`, `YR_RED`, `YR_GREEN`,
`YR_BLUE`, `YR_YELLOW`, `YR_PURPLE`, `YR_ORANGE`, `YR_CYAN`, `YR_PINK`,
`YR_GRAY`, `YR_SILVER`, `YR_MAROON`, `YR_DARK_RED`, `YR_DARK_GREEN`,
`YR_DARK_BLUE`, `YR_OLIVE`, `YR_TEAL`, `YR_NAVY`, `YR_BROWN`, `YR_SKY_BLUE`,
`YR_EMPTY_PIXEL`) are defined as `yr_pixel_t` values, in RGB565 or ARGB form
depending on whether `COLOR_565` is defined.

## Physics & Collisions (`physics.h`)

```c
Vector2 yr_move(Vector2 subject_position, Vector2 subject_direction, enum YrMovementDirection direction, float movement_speed); // Moves a point using delta time; direction is YR_FORWARD/BACK/LEFT/RIGHT
Vector2 yr_rotate(Vector2 vector, enum YrRotationDirection direction, float rotation_speed); // Rotates a vector using delta time; direction is YR_CLOCKWISE/COUNTERCLOCKWISE

YrCollisionInfo yr_check_collision(YrContext *ctx, Vector2 next_pos, float threshold, uint32_t collision_mask); // [macro] First collision (wall or entity) at next_pos
YrCollisionInfo yr_check_collision_out_radius(YrContext *ctx, Vector2 next_pos, float threshold, uint32_t collision_mask, float radius); // Same, ignoring entities farther than `radius` from next_pos
size_t yr_check_mult_collisions(YrContext *ctx, Vector2 next_pos, float threshold, uint32_t collision_mask, YrCollisionInfo *out_info, size_t len); // [macro] Fills out_info[] with every collision at next_pos, returns hit count
size_t yr_check_mult_collisions_out_radius(YrContext *ctx, Vector2 next_pos, float threshold, uint32_t collision_mask, float radius, YrCollisionInfo *out_info, size_t len); // Same, with an outer entity-search radius
YrCollisionInfo yr_check_ray_collision(YrContext *ctx, Vector2 origin, Vector2 dir, float threshold, uint32_t collision_mask); // First collision hit by a ray cast from origin along dir
Vector2 yr_slide_collision(YrContext *ctx, Vector2 from, Vector2 to, YrCollisionInfo *hit, float threshold, uint32_t collision_mask); // Slides from→to around the first obstacle; writes hit info to *hit, returns the slided new position
Vector2 yr_slide_collision_out_radius(YrContext *ctx, Vector2 from, Vector2 to, YrCollisionInfo *hit, float threshold, uint32_t collision_mask, float radius); // Same, with an outer entity-search radius
```

```c
typedef struct {
    YrCollisionType type;   // YR_COLLISION_NONE, YR_COLLISION_WALL, YR_COLLISION_ENTITY
    int cell_x, cell_y;     // map cell of the hit wall (wall hits only)
    uint8_t tile;           // tile value of the hit wall (wall hits only)
    YrEntity *entity;       // pointer to the hit entity (entity hits only)
    size_t entity_index;    // id of the hit entity in ctx->entities (entity hits only)
} YrCollisionInfo;
```

## Input (`inputs.h`)

```c
void  yr_esp_key_init(int pin, int key);                  // ESP32: maps a pull-up GPIO pin to a YARI key; no-op on desktop/web
int   yr_esp_joystick_init(int joystick_pin_x, int joystick_pin_y); // ESP32: registers two ADC pins as a joystick, returns its id; stub elsewhere
float yr_esp_joystick_get_axis(int joystick_id, int axis); // Reads YR_X_AXIS/YR_Y_AXIS roughly in -1..1; returns 0 on desktop/web
bool  yr_is_key_down(int key);                            // True while a key/GPIO is held down
bool  yr_is_key_up(int key);                              // True while a key/GPIO is released
bool  yr_is_key_pressed(int key);                         // True on the frame a key/GPIO transitions to pressed (edge-triggered)
```

Key codes are the `YrKeyboardKey` enum (`YR_KEY_A`, `YR_KEY_SPACE`,
`YR_KEY_ESCAPE`, `YR_KEY_UP/DOWN/LEFT/RIGHT`, `YR_KEY_F1..F12`, ...), values
matching raylib's key codes so desktop/web/ESP32 game code stays identical.
`YrJoystickAxis` provides `YR_X_AXIS` / `YR_Y_AXIS`.

## Utils - Timers (`yari_utils.h`)

```c
YrTimer yr_timer_start(float duration);                  // Returns a timer that expires `duration` seconds from now
bool    yr_timer_is_done(const YrTimer *timer);          // True once the timer has expired
bool    yr_timer_loop(YrTimer *loop, float duration);    // Fires and resets the timer; true each time it triggers
bool    yr_timer_is_started(const YrTimer *timer);       // True if the timer has been initialized
```

`timer->count` increments every time `yr_timer_loop` fires.

## Utils - Sprite Animation (`yari_utils.h`)

```c
void yr_start_animation(YrAnimationStack *stack, YrAnimation a, float pop_after); // Pushes animation `a` ({frames, frame_count, duration}); pop_after <= 0 loops forever
void yr_start_loop_animation(stack, a);         // [macro] Pushes a looping animation (pop_after = 0)
void yr_start_animation_once(stack, a);         // [macro] Pushes a one-shot animation that auto-pops after a.duration * a.frame_count seconds
int  yr_get_animation_texture(YrAnimationStack *animation); // Advances the top animation, pops on expiry (resuming the one beneath), returns its current frame's texture id, or -1 if the stack is empty
```

Entities carry their own `animation` (a `YrAnimationStack` field on `YrEntity`) which `yr_draw_entities` advances automatically every frame, writing the result into `entity->texture_id` - push onto `&self->animation` and the sprite animates with no extra per-frame call. Use `yr_get_animation_texture` directly only for animations not tied to an entity (e.g. a HUD weapon sprite).

## Utils - Entities (`yari.h`)

```c
size_t yr_create_entity_ex(YrContext *ctx, YrEntity e, void *data); // Runs e.init(&e, data) if set, inserts e into ctx->entities (a YrEntityMap), returns its new stable id
yr_create_entity(state, e);                              // [macro] yr_create_entity_ex(state, e, NULL)
void   yr_remove_entity(YrContext *ctx, size_t id);  // Runs entity->cleanup (if set), frees its animation stack, then removes the entity with this id
size_t yr_get_entity_id(YrEntity *e);                     // Recovers the id of a live entity pointer obtained from ctx->entities
```

`data` in `yr_create_entity_ex` is a spawn-time payload forwarded to `e.init`, separate from `entity_data` (which factories set directly from their own `data` parameter) - see README - `Entities`.

`ctx->entities` is a `yr_Hm(size_t, YrEntity)`; iterate with `yr_foreach(&ctx->entities, kv)` (`kv->key` is the id, `kv->value` the `YrEntity`). See [Hash Map & Hash Set](#hash-map--hash-set-hth) and README - `Entities`.

## Dynamic Array (`da.h`)

Generic macros over any struct shaped `{ Type *data; size_t length; size_t capacity; }` (e.g. `YrAnimationStack`). `yr_Hm`/`yr_Hs` (below) extend this same layout with a hash index, so read-only `yr_da_foreach` also works over hash maps and sets - but use the `yr_hm_*`/`yr_hs_*` mutators, not `yr_da_append`/`yr_da_remove_*` directly, or the hash index falls out of sync.

`YR_DA_INIT_CAPACITY` (4), `YR_DA_GROWTH_FACTOR` (1.5) and `YR_DA_SHRINK_FACTOR` (2) are `#define`d with `#ifndef` guards, so `#define` them before including `da.h`/`yari.h` to override.

```c
YR_ARRAY_LEN(array);                    // Number of elements in a fixed-size C array
yr_da_reserve(da, expected_capacity);   // Grows da->data to fit at least expected_capacity items
yr_da_append(da, item);                 // Appends item, growing storage as needed
yr_da_pop(da);                          // Removes the last element of the array and return a pointer to it (it does not auto-shrink the capacity)
yr_da_remove_unordered(da, idx);        // Removes item at idx by swapping in the last element - O(1), reorders; auto-shrinks storage if underfilled
yr_da_remove(da, idx, del);             // Removes `del` items at idx, preserving order (memmove); auto-shrinks storage if underfilled
yr_da_shrink(da);                       // Shrinks capacity by YR_DA_GROWTH_FACTOR (never below YR_DA_INIT_CAPACITY or length + 1); called automatically by the removals above
yr_da_foreach(da, var);                 // [macro] for-loop over da->data; var is a pointer to each element
yr_da_foreach_idx(da, idx);             // [macro] for-loop with size_t idx over [0, da->length)
yr_da_free(da);                         // Frees storage and resets length/capacity to 0
```

## Hash Map & Hash Set (`ht.h`)

Generic open-addressing hash map and set, included by `yari.h` (used internally for `ctx->entities`). `const char *`/`char *` keys (map) or values (set) are hashed/compared by content and heap-copied/freed internally; any other type is hashed/compared by raw bytes.

```c
yr_Hm(key_t, val_t)                // [type] anonymous hash map struct: { struct { key_t key; val_t value; } *data; length; capacity; ... }
yr_hm_declare(name, key_t, val_t); // [macro] typedefs a named yr_Hm(key_t, val_t)
yr_hm_set(hm, key, val);           // Inserts key -> val, or updates val if key already exists
void *yr_hm_try(hm, key);          // Pointer to the value for key, or NULL if absent
bool  yr_hm_has(hm, key);          // True if key is present
val_t yr_hm_get(hm, key);          // Value for key, or {0} if absent (use yr_hm_try when absence matters)
val_t *yr_hm_remove(hm, key);      // Removes key, returns a pointer to its (relocated) value, or NULL if absent
yr_hm_free(hm);                    // Frees the map; does not free keys/values you own
yr_hm_shrink(hm);                  // Shrinks storage to fit length (fully frees if empty)

yr_Hs(val_t)                       // [type] anonymous hash set struct: { val_t *data; length; capacity; ... }
yr_hs_declare(name, val_t);        // [macro] typedefs a named yr_Hs(val_t)
bool yr_hs_has(set, val);          // True if val is present
yr_hs_add(set, val);               // Adds val if not already present
bool yr_hs_remove(set, val);       // Removes val, returns true if it was present
yr_foreach(set, v);                // [macro] for-loop over entries; kv->key / kv->value or value for sets
yr_hs_cat(set, other_set);         // Adds every value of other_set into set
yr_hs_cat_da(set, da);             // Adds every value of a dynamic array into set
yr_hs_sub(set, other_set);         // Removes every value of other_set from set
yr_hs_sub_da(set, da);             // Removes every value of a dynamic array from set
yr_hs_to_da(set, da);              // Overwrites da with the set's values
yr_da_to_hs(da, set);              // Overwrites set with the array's values
yr_hs_free(set);                   // Frees the set
yr_hs_shrink(set);                 // Shrinks storage to fit length (fully frees if empty)
```
