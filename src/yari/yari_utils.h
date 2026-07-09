#ifndef YR_UTILS_H
#define YR_UTILS_H

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

YrTimer yr_timer_start(float duration);
bool yr_timer_is_done(const YrTimer *timer);
bool yr_timer_loop(YrTimer *loop, float duration);
bool yr_timer_is_started(const YrTimer *timer) ;

void yr_start_animation(YrAnimationStack *stack, YrAnimation a, float pop_after);
#define yr_start_loop_animation(stack, a) yr_start_animation(stack, a, 0.0f)
#define yr_start_animation_once(stack, a) yr_start_animation(stack, a, (a).duration * (a).frame_count)
int yr_get_animation_texture(YrAnimationStack *animation);

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
#endif

#endif // YR_UTILS_H
