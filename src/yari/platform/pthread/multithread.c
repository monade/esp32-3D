#ifdef YR_MULTITHREAD
#include "../../yari.h"

#include <pthread.h>
#include <stdlib.h>

// Number of threads participating in a yr_run_split: the caller plus this
// many persistent workers. Override at build time, e.g.
// -DYR_RENDER_THREAD_COUNT=4, to use more of a multi-core desktop machine.
// Defaults to 2 (1 worker), matching the ESP32 dual-core split, which stays
// hardwired to 2 since more OS threads than physical cores buys no
// parallelism there.
#ifndef YR_RENDER_THREAD_COUNT
#define YR_RENDER_THREAD_COUNT 4
#endif
#define YR_RENDER_WORKER_COUNT (YR_RENDER_THREAD_COUNT - 1)

// Desktop (raylib/SDL) counterpart to platform/esp32/multithread.c:
// YR_RENDER_WORKER_COUNT persistent worker pthreads each run one leading
// chunk of a split job while the caller thread runs the trailing chunk
// (which absorbs any remainder), then the caller waits for every worker to
// finish. Workers are spawned once and kept alive for the life of the
// process to avoid pthread_create overhead on every split. All shared
// yr_context must be read-only during the parallel phase and each chunk must
// only write data disjoint from the others.
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t start_cond;
    pthread_cond_t done_cond;
    bool job_ready;
    bool job_done;
    void (*job)(void *ctx, int start, int end);
    void *ctx;
    int start, end;
} yr_worker_slot_t;

static pthread_t yr_worker_threads[YR_RENDER_WORKER_COUNT];
static yr_worker_slot_t yr_worker_slots[YR_RENDER_WORKER_COUNT];
static bool yr_workers_started = false;
static bool yr_workers_failed = false;

static void *yr_render_worker(void *arg) {
    yr_worker_slot_t *slot = (yr_worker_slot_t *)arg;
    for (;;) {
        pthread_mutex_lock(&slot->mutex);
        while (!slot->job_ready) pthread_cond_wait(&slot->start_cond, &slot->mutex);
        slot->job_ready = false;
        pthread_mutex_unlock(&slot->mutex);

        slot->job(slot->ctx, slot->start, slot->end);

        pthread_mutex_lock(&slot->mutex);
        slot->job_done = true;
        pthread_cond_signal(&slot->done_cond);
        pthread_mutex_unlock(&slot->mutex);
    }
    return NULL;
}

static bool yr_render_workers_start(void) {
    if (yr_workers_started) return true;
    if (yr_workers_failed) return false;

    for (int i = 0; i < YR_RENDER_WORKER_COUNT; i++) {
        yr_worker_slot_t *slot = &yr_worker_slots[i];
        pthread_mutex_init(&slot->mutex, NULL);
        pthread_cond_init(&slot->start_cond, NULL);
        pthread_cond_init(&slot->done_cond, NULL);
        slot->job_ready = false;
        slot->job_done = false;
        if (pthread_create(&yr_worker_threads[i], NULL, yr_render_worker, slot) != 0) {
            yr_workers_failed = true;
            return false;
        }
    }
    yr_workers_started = true;
    return true;
}

// Runs job over [0, total): up to YR_RENDER_WORKER_COUNT workers each take an
// equal-sized leading chunk while the caller takes the trailing chunk (which
// absorbs any remainder), then the caller waits for every worker to finish.
// Uses fewer workers when total is smaller than the configured thread count,
// and falls back to one sequential call if the workers can't start. Call
// only from the main thread, never from inside another split job.
void yr_run_split(void (*job)(void *ctx, int start, int end), void *ctx, int total) {
    int workers = YR_RENDER_WORKER_COUNT;
    if (workers > total - 1) workers = total - 1;

    if (workers > 0 && yr_render_workers_start()) {
        int threads = workers + 1;
        int chunk = total / threads;

        for (int i = 0; i < workers; i++) {
            yr_worker_slot_t *slot = &yr_worker_slots[i];
            slot->job = job;
            slot->ctx = ctx;
            slot->start = i * chunk;
            slot->end = slot->start + chunk;

            pthread_mutex_lock(&slot->mutex);
            slot->job_done = false;
            slot->job_ready = true;
            pthread_cond_signal(&slot->start_cond);
            pthread_mutex_unlock(&slot->mutex);
        }

        job(ctx, workers * chunk, total);

        for (int i = 0; i < workers; i++) {
            yr_worker_slot_t *slot = &yr_worker_slots[i];
            pthread_mutex_lock(&slot->mutex);
            while (!slot->job_done) pthread_cond_wait(&slot->done_cond, &slot->mutex);
            pthread_mutex_unlock(&slot->mutex);
        }
    } else {
        job(ctx, 0, total);
    }
}

// Guards ctx->_sprites appends made by yr_raycast_walls (see yari.c), which
// run concurrently across all render threads during yr_draw_walls_half_job
// (see renderer_common.c) - a plain yr_da_append (realloc included) is
// not safe to call from multiple threads on the same buffer without this.
// The critical section is just the append itself (a bounds check, maybe a
// realloc, one struct copy), so contention is negligible even when several
// chunks hit a transparent wall in the same frame.
static pthread_mutex_t yr_sprites_mutex = PTHREAD_MUTEX_INITIALIZER;
void yr__sprites_lock(void) { pthread_mutex_lock(&yr_sprites_mutex); }
void yr__sprites_unlock(void) { pthread_mutex_unlock(&yr_sprites_mutex); }

#endif
