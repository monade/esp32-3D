#ifndef YR_RENDERER_H
#define YR_RENDERER_H
#include <stddef.h>
#include <stdbool.h>
#include "colors.h"

// Default screen/window size
// use ESP32 TTGO size by default
#ifndef YR_LCD_W
#define YR_LCD_W 240
#endif
#ifndef YR_LCD_H
#define YR_LCD_H 136
#endif

#define YR_FOV_ANGLE (PI / 3.5)
#ifndef YR_MAX_RENDER_DIST
#define YR_MAX_RENDER_DIST 20.0
#endif
#ifndef YR_TEXTURE_SIZE
#define YR_TEXTURE_SIZE 64
#endif

typedef enum {
    YR_FONT_SM,
    YR_FONT_MD,
    YR_FONT_LG,
    YR_FONT_XL
} yr_font_size_t;

// Mirrors stbtt_bakedchar: one entry per ASCII character baked into the atlas.
typedef struct {
    short x0, y0, x1, y1;  // atlas pixel bounds of the glyph
    float xoff, yoff;       // offset from cursor to top-left of glyph
    float xadvance;         // how far to advance the cursor after this glyph
} yr_glyph_t;

typedef struct {
    const uint8_t *atlas;   // 1-bit packed bitmap, LSB-first: pixel i → bit i%8 of byte i/8
    const yr_glyph_t *glyphs;  // glyph info for ASCII 32–127 (96 entries)
    int atlas_w, atlas_h;
    float size;
} yr_font_t;

void yr_draw_texture(int x, int y, int width, int height, const yr_pixel_t *texture, int texture_width, int texture_height, bool skip_empty);

void yr_draw_text(const char *text, int x, int y, const yr_font_t *font, yr_pixel_t c);
size_t yr_get_text_length(const char *text, size_t len, const yr_font_t *font);

float yr_get_frame_time();

float yr_get_time();

void yr_clear_screen(yr_pixel_t color);

int yr_screen_width();
int yr_screen_height();
void yr_fill_span(int x, int y, int width, yr_pixel_t color);

void yr_draw_rectangle(int x, int y, int width, int height, yr_pixel_t color);
#define yr_draw_pixel(x, y, color) yr_draw_rectangle(x, y, 1, 1, color)
void yr_draw_rectangle_line(int x, int y, int width, int height, int thickness, yr_pixel_t color);
void yr_draw_line(int x0, int y0, int x1, int y1, int thickness, yr_pixel_t color);
void yr_draw_circle(int x, int y, int radius, yr_pixel_t color);
void yr_draw_circle_line(int x, int y, int radius, int thickness, yr_pixel_t color);
void yr_draw_triangle(int x0, int y0, int x1, int y1, int x2, int y2, yr_pixel_t color);
void yr_draw_triangle_line(int x0, int y0, int x1, int y1, int x2, int y2, int thickness, yr_pixel_t color);

void yr_renderer_init(int width, int height, const char *title, unsigned int target_fps);

bool yr_game_should_close();

void yr_begin_drawing();

void yr_render_screen();

void yr_end_drawing();

float yr_get_fps();

yr_pixel_t *get_framebuffer();

typedef void (*YrColorFilterCallback)(int x, int y, yr_pixel_t *color, void *user_data);
void yr_apply_color_filter(YrColorFilterCallback apply, void *user_data);

#ifdef ESP32_MULTITHREAD
// Runs job over [0, total) split across the two ESP32 cores (defined in
// yari.c): the worker runs [0, total/2) while the caller runs the rest.
// The job runs concurrently on both cores, so it must only write data
// disjoint per range. Call from the main task only, never from inside
// another split job.
void yr_run_split(void (*job)(void *ctx, int start, int end), void *ctx, int total);
#endif

#ifdef YARI_NO_PREFIX
#define draw_texture yr_draw_texture
#define draw_text yr_draw_text
#define get_text_length yr_get_text_length
#define screen_width yr_screen_width
#define screen_height yr_screen_height
#define get_frame_time yr_get_frame_time
#define get_time yr_get_time
#define clear_screen yr_clear_screen
#define draw_rectangle yr_draw_rectangle
#define draw_pixel yr_draw_pixel
#define draw_rectangle_line yr_draw_rectangle_line
#define draw_line yr_draw_line
#define draw_circle yr_draw_circle
#define draw_circle_line yr_draw_circle_line
#define draw_triangle yr_draw_triangle
#define draw_triangle_line yr_draw_triangle_line
#define renderer_init yr_renderer_init
#define game_should_close yr_game_should_close
#define begin_drawing yr_begin_drawing
#define render_screen yr_render_screen
#define end_drawing yr_end_drawing
#define get_fps yr_get_fps
#define ColorFilter YrColorFilter
#define apply_color_filter yr_apply_color_filter
#endif

#endif // YR_RENDERER_H
