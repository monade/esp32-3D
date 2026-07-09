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

static void fail_sdl(const char *message) {
    fprintf(stderr, "SDL backend error: %s: %s\n", message, SDL_GetError());
    exit(1);
}

static void yr_sdl_shutdown(void) {
    free(framebuffer);
    framebuffer = NULL;

    if (frame_texture) SDL_DestroyTexture(frame_texture);
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);

    frame_texture = NULL;
    renderer = NULL;
    window = NULL;
    SDL_Quit();
}

static uint32_t sdl_pixel_format(void) {
#ifdef COLOR_565
    return SDL_PIXELFORMAT_RGB565;
#else
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
    SDL_UpdateTexture(
        frame_texture,
        NULL,
        framebuffer,
        framebuffer_width * (int)sizeof(framebuffer[0])
    );
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

void yr_draw_rectangle(int x, int y, int width, int height, yr_pixel_t color) {
    if (width <= 0 || height <= 0 || !framebuffer) return;

    if (x < 0) {
        width += x;
        x = 0;
    }
    if (y < 0) {
        height += y;
        y = 0;
    }
    if (x + width > framebuffer_width) width = framebuffer_width - x;
    if (y + height > framebuffer_height) height = framebuffer_height - y;
    if (width <= 0 || height <= 0) return;

    for (int row = 0; row < height; row++) {
        yr_pixel_t *dst = &framebuffer[(y + row) * framebuffer_width + x];
        for (int col = 0; col < width; col++) dst[col] = color;
    }
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
