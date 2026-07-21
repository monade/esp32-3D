#include <stdbool.h>
#include "yari_utils.h"

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

// Denormalize screen coordinates
Vector2 yr_screen_coord(Vector2 pos) {
    Vector2 center = {(yr_screen_width()-1)/2.0f, (yr_screen_height()-1)/2.0f};
    return (Vector2) {pos.x * center.x + center.x, pos.y * center.y + center.y};
}

Vector2 yr_screen_coord_abs(Vector2 pos) {
    Vector2 center = {yr_screen_width()-1, yr_screen_height()-1};
    return (Vector2) {pos.x * center.x, pos.y * center.y};
}

void yr__draw_text_ex(const char *txt, const yr_font_t *font, struct yr_dtxt param) {
    size_t len = param.length == 0 ? strlen(txt): param.length;
    Vector2 dx = param.display == YR_LAY_SCREEN ? (Vector2){param.x, param.y} : yr_screen_coord_abs((Vector2){param.nx, param.ny});
    float h = font->size;
    float hh = h/2.0f;
    if(param.box.width == 0 && param.box.height == 0) {
        param.box.width = yr_screen_width();
        param.box.height = yr_screen_height();
    }

    switch(param.align) {
    case YR_LAY_CENTER: {
        float w = yr_get_text_length(txt, len, font);
        float x = (param.box.width - w + dx.x)/2.0f;
        float y = param.box.height/2.0f + dx.y + hh;
        yr_draw_text(txt, x, y, font, param.color);
    } break;
    case YR_LAY_CB: {
        float w = yr_get_text_length(txt, len, font);
        float x = (param.box.width - w + dx.x)/2.0f;
        float y = param.box.height + dx.y;
        yr_draw_text(txt, x, y, font, param.color);
    } break;
    case YR_LAY_CT: {
        float w = yr_get_text_length(txt, len, font);
        float x = (param.box.width - w + dx.x)/2.0f;
        yr_draw_text(txt, x, dx.y + h, font, param.color);
    } break;
    case YR_LAY_CL: {
        float y = param.box.height/2.0f + dx.y + hh;
        yr_draw_text(txt, dx.x, y, font, param.color);
    } break;
    case YR_LAY_CR: {
        float w = yr_get_text_length(txt, len, font);
        float x = param.box.width - w + dx.x;
        float y = param.box.height/2.0f + dx.y + hh;
        yr_draw_text(txt, x, y, font, param.color);
    } break;
    case YR_LAY_NONE:
    case YR_LAY_TL: {
        yr_draw_text(txt, dx.x, dx.y + h, font, param.color);
    } break;
    case YR_LAY_TR: {
        float x = param.box.width - yr_get_text_length(txt, len, font) + dx.x;
        yr_draw_text(txt, x, dx.y + h, font, param.color);
    } break;
    case YR_LAY_BR: {
        float x = param.box.width - yr_get_text_length(txt, len, font) + dx.x;
        float y = param.box.height + dx.y;
        yr_draw_text(txt, x, y, font, param.color);
    } break;
    case YR_LAY_BL: {
        float y = param.box.height + dx.y;
        yr_draw_text(txt, dx.x, y, font, param.color);
    } break;
    }
}

void yr__draw_texture_ex(const yr_pixel_t *texture, size_t txw, size_t txh, struct yr_dtex param) {
    Vector2 dx = param.display == YR_LAY_SCREEN ? (Vector2){param.x, param.y} : yr_screen_coord_abs((Vector2){param.nx, param.ny});
    if (param.box.width == 0 && param.box.height == 0) {
        param.box.width = yr_screen_width();
        param.box.height = yr_screen_height();
    }
    if (param.width == 0 && param.height == 0) {
        param.width = txw;
        param.height = txh;
    } else if (param.width == 0) {
        param.width = (txw*param.height)/txh;
    } else if (param.height == 0) {
        param.height = (txh*param.width)/txw;
    }
    switch(param.align) {
    case YR_LAY_CENTER: {
        float x = (param.box.width - (param.width - dx.x))/2.0f;
        float y = (param.box.height - (param.height - dx.y))/2.0f;
        yr_draw_texture(x, y, param.width, param.height, texture, txw, txh, !param.draw_empty);
    } break;
    case YR_LAY_CB: {
        float x = (param.box.width - param.width + dx.x)/2.0f;
        float y = param.box.height - param.height + dx.y;
        yr_draw_texture(x, y, param.width, param.height, texture, txw, txh, !param.draw_empty);
    } break;
    case YR_LAY_CT: {
        float x = (param.box.width - param.width + dx.x)/2.0f;
        yr_draw_texture(x, dx.y, param.width, param.height, texture, txw, txh, !param.draw_empty);
    } break;
    case YR_LAY_CL: {
        float y = (param.box.height - param.height + dx.y)/2.0f;
        yr_draw_texture(dx.x, y, param.width, param.height, texture, txw, txh, !param.draw_empty);
    } break;
    case YR_LAY_CR: {
        float x = param.box.width - param.width + dx.x;
        float y = (param.box.height - param.height + dx.y)/2.0f;
        yr_draw_texture(x, y, param.width, param.height, texture, txw, txh, !param.draw_empty);
    } break;
    case YR_LAY_NONE:
    case YR_LAY_TL: {
        yr_draw_texture(dx.x, dx.y, param.width, param.height, texture, txw, txh, !param.draw_empty);
    } break;
    case YR_LAY_TR: {
        float x = param.box.width - param.width + dx.x;
        yr_draw_texture(x, dx.y, param.width, param.height, texture, txw, txh, !param.draw_empty);
    } break;
    case YR_LAY_BR: {
        float x = param.box.width - param.width + dx.x;
        float y = param.box.height - param.height + dx.y;
        yr_draw_texture(x, y, param.width, param.height, texture, txw, txh, !param.draw_empty);
    } break;
    case YR_LAY_BL: {
        float y = param.box.height - param.height + dx.y;
        yr_draw_texture(dx.x, y, param.width, param.height, texture, txw, txh, !param.draw_empty);
    } break;
    }
}
