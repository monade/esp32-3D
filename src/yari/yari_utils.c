#include <stdbool.h>
#include "yari_utils.h"
#include "renderer.h"
#include "da.h"

YrTimer yr_timer_start(float duration) {
    YrTimer timer;
    timer.ends_at = yr_get_time() + duration;
    timer.count = 0;
    return timer;
}

bool yr_timer_is_done(const YrTimer *timer) {
    return yr_get_time() >= timer->ends_at;
}

bool yr_timer_loop(YrTimer *loop, float duration) {
    float now = yr_get_time();
    if (now >= loop->ends_at) {
        loop->ends_at = now + duration;
        loop->count++;
        return true;
    }
    return false;
}

bool yr_timer_is_started(const YrTimer *timer) {
    return timer->ends_at != 0.0f;
}

void yr_start_animation(YrAnimationStack *stack, YrAnimation a, float pop_after) {
    YrAnimationData anim = {0};
    anim.frames = a.frames;
    anim.frame_count = a.frame_count;
    anim.duration = a.duration;
    if (pop_after > 0.0f) anim.pop_time = yr_get_time() + pop_after;
    yr_da_append(stack, anim);
}


int yr_get_animation_texture(YrAnimationStack *animation) {
    YrAnimationData *anim;
    float now = yr_get_time();
start:
    if (animation->length <= 0) return -1;
    anim = &animation->data[animation->length - 1];
    if (anim->pop_time > 0.0f && now >= anim->pop_time) {
        animation->length--;
        goto start;
    }
    yr_timer_loop(&anim->timer, anim->duration);
    int frame_index = anim->timer.count % anim->frame_count;
    return anim->frames[frame_index];
}