#include "../../renderer.h"
#include <raylib.h>
#include <math.h>

static Image frame_buffer;
static Texture2D frame_texture;

float yr_get_frame_time() {
    return GetFrameTime();
}

void yr_draw_rectangle(int x, int y, int width, int height, yr_pixel_t color) {
    ImageDrawRectangle(&frame_buffer, x, y, width, height, GetColor(color));
}

void yr_clear_screen(yr_pixel_t color) {
    ImageClearBackground(&frame_buffer, GetColor(color));
}

void yr_apply_color_filter(YrColorFilterCallback apply, void *user_data) {
    if (!apply || !frame_buffer.data) return;

    // GenImageColor gives an R8G8B8A8 image: repack each Color into the
    // 0xRRGGBBAA yr_pixel_t layout for the filter and back.
    Color *px = frame_buffer.data;
    for (int y = 0; y < frame_buffer.height; y++) {
        for (int x = 0; x < frame_buffer.width; x++, px++) {
            yr_pixel_t c = ((yr_pixel_t)px->r << 24)
                         | ((yr_pixel_t)px->g << 16)
                         | ((yr_pixel_t)px->b << 8)
                         | px->a;
            apply(x, y, &c, user_data);
            px->r = (unsigned char)(c >> 24);
            px->g = (unsigned char)(c >> 16);
            px->b = (unsigned char)(c >> 8);
            px->a = (unsigned char)c;
        }
    }
}

yr_pixel_t *get_framebuffer() {
    return (yr_pixel_t *)frame_buffer.data;
}

void yr_renderer_init(int width, int height, const char *title, unsigned int target_fps) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(width, height, title);
    if(target_fps > 0) SetTargetFPS(target_fps);
    SetTraceLogLevel(LOG_WARNING);
    frame_buffer = GenImageColor(width, height, BLACK);
    frame_texture = LoadTextureFromImage(frame_buffer);
}

bool yr_game_should_close() {
    return WindowShouldClose();
}

void yr_begin_drawing() {
    BeginDrawing();
}
  
void yr_render_screen() {
    UpdateTexture(frame_texture, frame_buffer.data);

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
