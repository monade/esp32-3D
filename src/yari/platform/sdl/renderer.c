#include "../../renderer.h"

#include <SDL.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static SDL_Window *window = NULL;
static SDL_Renderer *renderer = NULL;
static SDL_Texture *frame_texture = NULL;
static yr_pixel_t *framebuffer = NULL;
static int framebuffer_width = 0;
static int framebuffer_height = 0;
static unsigned int target_fps = 60;
static double performance_freq = 0.0;
static uint64_t last_frame_start = 0;
static uint64_t frame_start = 0;
static float cached_frame_time = 0.0f;

#if defined(YR_L8)
// SDL2 has no accelerated-texture-safe 8bpp pixel format (indexed textures
// aren't reliably supported by streaming/accelerated renderers), so the
// grayscale framebuffer is expanded into this RGBA8888 buffer on upload.
static uint32_t *upload_buffer = NULL;
#endif

static void fail_sdl(const char *message) {
    fprintf(stderr, "SDL backend error: %s: %s\n", message, SDL_GetError());
    exit(1);
}

static void yr_sdl_shutdown(void) {
    free(framebuffer);
    framebuffer = NULL;

#if defined(YR_L8)
    free(upload_buffer);
    upload_buffer = NULL;
#endif

    if (frame_texture) SDL_DestroyTexture(frame_texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);

    frame_texture = NULL;
    renderer = NULL;
    window = NULL;
    SDL_Quit();
}

static uint32_t sdl_pixel_format(void) {
#if defined(YR_RGB565)
    return SDL_PIXELFORMAT_RGB565;
#else
    // YR_L8 also lands here: the framebuffer is expanded to RGBA8888 on
    // upload (see yr_render_screen), same as the plain 32-bit ARGB format.
    return SDL_PIXELFORMAT_RGBA8888;
#endif
}

static uint64_t now_counter(void) {
    return SDL_GetPerformanceCounter();
}

void yr_renderer_init(int width, int height, const char *title, unsigned int fps) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) {
        fail_sdl("SDL_Init");
    }

    framebuffer_width = width;
    framebuffer_height = height;
    target_fps = fps;
    performance_freq = (double)SDL_GetPerformanceFrequency();

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");

    window = SDL_CreateWindow(
        title ? title : "Yari",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        width,
        height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window) fail_sdl("SDL_CreateWindow");

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) fail_sdl("SDL_CreateRenderer");

    if (SDL_RenderSetLogicalSize(renderer, width, height) != 0) {
        fail_sdl("SDL_RenderSetLogicalSize");
    }

    frame_texture = SDL_CreateTexture(
        renderer,
        sdl_pixel_format(),
        SDL_TEXTUREACCESS_STREAMING,
        width,
        height
    );
    if (!frame_texture) fail_sdl("SDL_CreateTexture");

    framebuffer = malloc((size_t)width * (size_t)height * sizeof(framebuffer[0]));
    if (!framebuffer) {
        fprintf(stderr, "SDL backend error: framebuffer allocation failed\n");
        exit(1);
    }

#if defined(YR_L8)
    upload_buffer = malloc((size_t)width * (size_t)height * sizeof(uint32_t));
    if (!upload_buffer) {
        fprintf(stderr, "SDL backend error: upload buffer allocation failed\n");
        exit(1);
    }
#endif

    atexit(yr_sdl_shutdown);
}

void yr_begin_drawing(void) {
    uint64_t now = now_counter();
    cached_frame_time = last_frame_start == 0
        ? 0.0f
        : (float)((double)(now - last_frame_start) / performance_freq);
    last_frame_start = now;
    frame_start = now;
}

void yr_render_screen(void) {
#if defined(YR_L8)
    int count = framebuffer_width * framebuffer_height;
#if defined(YR_MONOCROME)
    for (int i = 0; i < count; i++)
        framebuffer[i] = yr_mono_dither_lit(framebuffer[i], i % framebuffer_width, i / framebuffer_width) ? 255 : 0;
#endif
    for (int i = 0; i < count; i++) {
        uint32_t v = framebuffer[i];
        upload_buffer[i] = (v << 24) | (v << 16) | (v << 8) | 0xFF;
    }
    SDL_UpdateTexture(
        frame_texture,
        NULL,
        upload_buffer,
        framebuffer_width * (int)sizeof(uint32_t)
    );
#else
    SDL_UpdateTexture(
        frame_texture,
        NULL,
        framebuffer,
        framebuffer_width * (int)sizeof(framebuffer[0])
    );
#endif
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, frame_texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void yr_end_drawing(void) {
    if (target_fps == 0) return;

    uint64_t now = now_counter();
    double elapsed = (double)(now - frame_start) / performance_freq;
    double target = 1.0 / (double)target_fps;

    if (elapsed < target) {
        uint32_t delay_ms = (uint32_t)((target - elapsed) * 1000.0);
        if (delay_ms > 0) SDL_Delay(delay_ms);
    }
}

float yr_get_frame_time(void) {
    return cached_frame_time;
}

float yr_get_time(void) {
    return (float)((double)now_counter() / performance_freq);
}

float yr_get_fps(void) {
    if (cached_frame_time <= 0.0f) return 0.0f;
    return 1.0f / cached_frame_time;
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

void yr_apply_color_filter(YrColorFilterCallback apply, void *user_data) {
    if (!apply || !framebuffer) return;

    yr_pixel_t *px = framebuffer;
    for (int y = 0; y < framebuffer_height; y++) {
        for (int x = 0; x < framebuffer_width; x++, px++) {
            apply(x, y, px, user_data);
        }
    }
}

yr_pixel_t *get_framebuffer(void) {
    return framebuffer;
}
