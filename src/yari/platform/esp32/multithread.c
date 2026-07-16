#ifdef ESP32_MULTITHREAD
#include "yari.h"

#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Dual-core work splitting: a worker task pinned to the other core runs the
// first half of a job while the main core runs the second half. Used for the
// screen rendering (split by columns) and yr_apply_color_filter (split by
// rows). All shared yr_context must be read-only during the parallel phase and
// each half must only write data disjoint from the other.
static TaskHandle_t yr_render_worker_handle = NULL;
static TaskHandle_t yr_render_main_handle = NULL;
static bool yr_render_worker_failed = false;
static void (*yr_par_job)(void *ctx, int start, int end) = NULL;
static void *yr_par_ctx = NULL;
static int yr_par_mid = 0;
static YrEntity **yr_par_entities = NULL;
static size_t yr_par_entity_count = 0;

static void yr_render_worker(void *arg) {
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        yr_par_job(yr_par_ctx, 0, yr_par_mid);
        xTaskNotifyGive(yr_render_main_handle);
    }
}

static bool yr_render_worker_start(void) {
    if (yr_render_worker_handle) return true;
    if (yr_render_worker_failed) return false;

    yr_render_main_handle = xTaskGetCurrentTaskHandle();

    // The main task runs on core 0 (ESP_MAIN_TASK_AFFINITY_CPU0); pin the
    // render worker to core 1. On unicore configs this fails and we fall
    // back to single-core execution.
    if (xTaskCreatePinnedToCore(yr_render_worker, "yr_render", 4096, NULL,
                                uxTaskPriorityGet(NULL), &yr_render_worker_handle, 1) != pdPASS) {
        yr_render_worker_handle = NULL;
        yr_render_worker_failed = true;
        return false;
    }
    return true;
}

// Runs job over [0, total): the worker executes [0, total/2) on core 1 while
// the caller executes [total/2, total), then they join. Falls back to one
// sequential call if the worker can't start. Call only from the main task,
// never from inside another split job.
void yr_run_split(void (*job)(void *ctx, int start, int end), void *ctx, int total) {
    if (total > 1 && yr_render_worker_start()) {
        yr_par_job = job;
        yr_par_ctx = ctx;
        yr_par_mid = total / 2;
        xTaskNotifyGive(yr_render_worker_handle);
        job(ctx, yr_par_mid, total);
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    } else {
        job(ctx, 0, total);
    }
}

// Draws background, walls and sprites for a range of screen columns. Units
// are ray_res-wide columns; the last unit absorbs any screen-width remainder.
static void yr_draw_half_job(void *_ctx, int start, int end) {
    YrContext *ctx = (YrContext *)_ctx;
    int rr = ctx->ray_res;
    int total = ctx->screen_width / rr;
    int x0 = start * rr;
    int x1 = (end == total) ? ctx->screen_width : end * rr;
    yr__draw_background_range(ctx, x0, x1);
    yr__draw_walls_range(ctx, x0, x1);
    yr__draw_sprites_range(ctx, yr_par_entities, yr_par_entity_count, x0, x1);
}

void yr__draw_game_multithread(YrContext *ctx) {
    YrEntity **entities = NULL;
    size_t entity_count = yr__entities_prep(ctx, &entities);
    yr_par_entities = entities;
    yr_par_entity_count = entity_count;
    yr_run_split(yr_draw_half_job, ctx, ctx->screen_width / ctx->ray_res);
    free(entities);
    yr__update_entities(ctx);
}

#endif


