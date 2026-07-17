#ifndef YR_UTILS_H
#define YR_UTILS_H

#define RAYMATH_STATIC_INLINE
#include "raymath.h"
#include "renderer.h"
#include "da.h"
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    float ends_at;
    unsigned int count;
} YrTimer;

typedef struct {
    YrTimer timer;
    const int *frames;
    size_t frame_count;
    float duration;
    float pop_time;
} YrAnimationData;

typedef struct {
    const int *frames;
    size_t frame_count;
    float duration;
} YrAnimation;

typedef struct {
    YrAnimationData *data;
    size_t length;
    size_t capacity;
} YrAnimationStack;


enum yr_lay_pos {
    YR_LAY_NONE,
    YR_LAY_CENTER,
    YR_LAY_CB,
    YR_LAY_CT,
    YR_LAY_CL,
    YR_LAY_CR,
    YR_LAY_TL,
    YR_LAY_TR,
    YR_LAY_BR,
    YR_LAY_BL,
};

enum yr_lay_dis {
    YR_LAY_SCREEN,
    YR_LAY_NORM,
};
struct yr_dtxt {
    yr_pixel_t color;
    enum yr_lay_pos align;
    enum yr_lay_dis display;
    struct {int width, height;} box;
    size_t length;
    union {
        struct {float nx, ny;};
        struct {int x, y;};
    };
};

struct yr_dtex {
    enum yr_lay_pos align;
    enum yr_lay_dis display;
    int width, height;
    struct {int width, height;} box;
    bool draw_empty;
    union {
        struct {float nx, ny;};
        struct {int x, y;};
    };
};

YrTimer yr_timer_start(float duration);
bool yr_timer_is_done(const YrTimer *timer);
bool yr_timer_loop(YrTimer *loop, float duration);
bool yr_timer_is_started(const YrTimer *timer) ;

void yr_start_animation(YrAnimationStack *stack, YrAnimation a, float pop_after);
#define yr_start_loop_animation(stack, a) yr_start_animation(stack, a, 0.0f)
#define yr_start_animation_once(stack, a) yr_start_animation(stack, a, (a).duration * (a).frame_count)
int yr_get_animation_texture(YrAnimationStack *animation);


Vector2 yr_screen_coord(Vector2 pos);
Vector2 yr_screen_coord_abs(Vector2 pos);

void yr__draw_text_ex(const char *txt, const yr_font_t *font, struct yr_dtxt param);
void yr__draw_texture_ex(const yr_pixel_t *texture, size_t txw, size_t txh, struct yr_dtex param);
#define yr_draw_text_ex(txt, font, ...) yr__draw_text_ex(txt, font, (struct yr_dtxt){__VA_ARGS__})
#define yr_draw_texture_ex(texture, txw, txh, ...) yr__draw_texture_ex(texture, txw, txh, (struct yr_dtex){__VA_ARGS__})


#ifdef YARI_NO_PREFIX
#define Timer YrTimer
#define timer_start yr_timer_start
#define timer_is_done yr_timer_is_done
#define timer_loop yr_timer_loop
#define timer_is_started yr_timer_is_started
#define AnimationData YrAnimationData
#define Animation YrAnimation
#define AnimationStack YrAnimationStack
#define start_animation yr_start_animation
#define start_loop_animation yr_start_loop_animation
#define start_animation_once yr_start_animation_once
#define get_animation_texture yr_get_animation_texture
#define screen_coord yr_screen_coord
#define screen_coord_abs yr_screen_coord_abs
#define draw_text_ex yr_draw_text_ex
#define draw_texture_ex yr_draw_texture_ex
#endif

#endif // YR_UTILS_H
