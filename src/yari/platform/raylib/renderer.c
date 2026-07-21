#include "../../renderer.h"
#include <raylib.h>
#include <math.h>
#include <stdlib.h>

static yr_pixel_t *framebuffer = NULL;
static int framebuffer_width = 0;
static int framebuffer_height = 0;
static Texture2D frame_texture;

// yr_pixel_t's bit layout (see colors.h) matches these raylib formats
// exactly, so the framebuffer uploads to the GPU with no per-pixel
// conversion, the same way the ESP32 backend blits its native format.
static int display_pixel_format(void) {
#if defined(YR_RGB565)
    return PIXELFORMAT_UNCOMPRESSED_R5G6B5;
#elif defined(YR_L8)
    return PIXELFORMAT_UNCOMPRESSED_GRAYSCALE;
#else
    return PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
#endif
}

float yr_get_frame_time() {
    return GetFrameTime();
}

int yr_screen_width(void) {
    return framebuffer_width;
}

int yr_screen_height(void) {
    return framebuffer_height;
}

// Precondition (guaranteed by yr_draw_rectangle in renderer_common.c): the
// whole [x, x+width) run at row y is in bounds.
void yr_fill_span(int x, int y, int width, yr_pixel_t color) {
    yr_pixel_t *dst = &framebuffer[y * framebuffer_width + x];
    for (int col = 0; col < width; col++) dst[col] = color;
}

void yr_clear_screen(yr_pixel_t color) {
    if (!framebuffer) return;

    int count = framebuffer_width * framebuffer_height;
    for (int i = 0; i < count; i++) framebuffer[i] = color;
}

typedef struct {
    YrColorFilterCallback apply;
    void *user_data;
} yr_filter_job_ctx;

// Applies the filter to framebuffer rows [y_start, y_end). With
// YR_MULTITHREAD this runs concurrently on the render worker thread over
// disjoint row ranges, so the callback must be safe to call from either side.
static void yr_filter_rows(void *arg, int y_start, int y_end) {
    const yr_filter_job_ctx *ctx = (const yr_filter_job_ctx *)arg;

    yr_pixel_t *px = framebuffer + (size_t)y_start * framebuffer_width;
    for (int y = y_start; y < y_end; y++) {
        for (int x = 0; x < framebuffer_width; x++, px++) {
            ctx->apply(x, y, px, ctx->user_data);
        }
    }
}

void yr_apply_color_filter(YrColorFilterCallback apply, void *user_data) {
    if (!apply || !framebuffer) return;

    yr_filter_job_ctx ctx = { .apply = apply, .user_data = user_data };
#ifdef YR_MULTITHREAD
    yr_run_split(yr_filter_rows, &ctx, framebuffer_height);
#else
    yr_filter_rows(&ctx, 0, framebuffer_height);
#endif
}

yr_pixel_t *get_framebuffer() {
    return framebuffer;
}

void yr_renderer_init(int width, int height, const char *title, unsigned int target_fps) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(width, height, title);
    if(target_fps > 0) SetTargetFPS(target_fps);
    SetTraceLogLevel(LOG_WARNING);

    framebuffer_width = width;
    framebuffer_height = height;
    framebuffer = calloc((size_t)width * (size_t)height, sizeof(yr_pixel_t));

    Image src = {
        .data = framebuffer,
        .width = width,
        .height = height,
        .mipmaps = 1,
        .format = display_pixel_format(),
    };
    frame_texture = LoadTextureFromImage(src);
}

bool yr_game_should_close() {
    return WindowShouldClose();
}

void yr_begin_drawing() {
    BeginDrawing();
}

void yr_render_screen() {
#if defined(YR_MONOCROME)
    int count = framebuffer_width * framebuffer_height;
    for (int i = 0; i < count; i++)
        framebuffer[i] = yr_mono_dither_lit(framebuffer[i], i % framebuffer_width, i / framebuffer_width) ? 255 : 0;
    UpdateTexture(frame_texture, framebuffer);
#elif !defined(YR_RGB565) && !defined(YR_L8)
    int count = framebuffer_width * framebuffer_height;
    uint32_t *pixels = (uint32_t *)framebuffer;
    for (int i = 0; i < count; i++) {
        uint32_t c = pixels[i];
        pixels[i] = __builtin_bswap32(c);
    }
    UpdateTexture(frame_texture, framebuffer);
#else
    UpdateTexture(frame_texture, framebuffer);
#endif

    int screen_w = GetScreenWidth();
    int screen_h = GetScreenHeight();
    float scale = fminf((float)screen_w / frame_texture.width, (float)screen_h / frame_texture.height);
    float dst_w = frame_texture.width * scale;
    float dst_h = frame_texture.height * scale;
    ClearBackground(BLACK);

    Rectangle src = { 0, 0, (float)frame_texture.width, (float)frame_texture.height };
    Rectangle dst = { (screen_w - dst_w) * 0.5f, (screen_h - dst_h) * 0.5f, dst_w, dst_h };
    DrawTexturePro(frame_texture, src, dst, (Vector2){ 0, 0 }, 0.0f, WHITE);
}

void yr_end_drawing() {
    EndDrawing();
}

float yr_get_time() {
    return (float)GetTime();
}

float yr_get_fps() {
    return (float)GetFPS();
}
