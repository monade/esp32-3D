#ifdef YR_MULTITHREAD
#include "yari.h"

#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Dual-core work splitting: a worker task pinned to the other core runs the
// first half of a job while the main core runs the second half. Used for the
// screen rendering (split by columns, see renderer_common.c) and
// yr_apply_color_filter (split by rows). All shared yr_context must be
// read-only during the parallel phase and each half must only write data
// disjoint from the other.
static TaskHandle_t yr_render_worker_handle = NULL;
static TaskHandle_t yr_render_main_handle = NULL;
static bool yr_render_worker_failed = false;
static void (*yr_par_job)(void *ctx, int start, int end) = NULL;
static void *yr_par_ctx = NULL;
static int yr_par_mid = 0;

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

// Guards ctx->_sprites appends made by yr_raycast_walls (see yari.c), which
// runs concurrently on both cores during yr_draw_walls_half_job (see
// renderer_common.c) - a plain yr_da_append (realloc included) is not
// safe to call from two cores on the same buffer without this. The critical
// section is just the append itself (a bounds check, maybe a realloc, one
// struct copy), so contention is negligible even when both cores hit a
// transparent wall in the same frame.
static portMUX_TYPE yr_sprites_mux = portMUX_INITIALIZER_UNLOCKED;
void yr__sprites_lock(void) { portENTER_CRITICAL(&yr_sprites_mux); }
void yr__sprites_unlock(void) { portEXIT_CRITICAL(&yr_sprites_mux); }

#endif
