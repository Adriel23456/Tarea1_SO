#include "scheduler.h"
#include "image_processing.h"
#include "logging.h"
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>

// ---- Min-heap ordered by ascending total_size ----
typedef struct {
    ProcJob* data;
    size_t   size;
    size_t   cap;
} JobHeap;

static JobHeap         g_heap = {0};
static pthread_mutex_t g_mtx  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  g_cv   = PTHREAD_COND_INITIALIZER;
static pthread_t       g_worker;
static atomic_int      g_running = 0;

static int  heap_reserve(JobHeap* h, size_t need);
static void heap_sift_up(JobHeap* h, size_t idx);
static void heap_sift_down(JobHeap* h, size_t idx);
static int  heap_push(JobHeap* h, const ProcJob* j);
static int  heap_pop_min(JobHeap* h, ProcJob* out);
static void free_job(ProcJob* j);
static void* worker_main(void* arg);

/*
 * scheduler_init
 * --------------
 * Initialize the background scheduler and start the worker thread.
 * Returns 0 on success, -1 on failure.
 */
int scheduler_init(void) {
    pthread_mutex_lock(&g_mtx);
    g_heap.data = NULL;
    g_heap.size = 0;
    g_heap.cap  = 0;
    g_running = 1;
    pthread_mutex_unlock(&g_mtx);

    int rc = pthread_create(&g_worker, NULL, worker_main, NULL);
    if (rc != 0) {
        g_running = 0;
        log_line("Scheduler: failed to start worker thread (rc=%d)", rc);
        return -1;
    }
    log_line("Scheduler: worker thread started");
    return 0;
}

/*
 * scheduler_enqueue
 * -----------------
 * Enqueue a processing job. The ProcJob structure's `data` buffer is
 * assumed to be owned by the caller and will become owned by the
 * scheduler on successful enqueue (the worker will free it).
 * Returns 0 on success, -1 on error.
 */
int scheduler_enqueue(const ProcJob* job) {
    if (!job || !job->data || job->size == 0) return -1;

    pthread_mutex_lock(&g_mtx);
    if (!g_running) { pthread_mutex_unlock(&g_mtx); return -1; }

    if (heap_push(&g_heap, job) != 0) {
        pthread_mutex_unlock(&g_mtx);
        log_line("Scheduler: heap_push failed (OOM?)");
        return -1;
    }
    pthread_cond_signal(&g_cv);
    pthread_mutex_unlock(&g_mtx);

    log_line("Scheduler: enqueued id=%s size=%u file=%s fmt=%s",
             job->image_id, job->total_size, job->filename, job->format);
    return 0;
}

/*
 * scheduler_shutdown
 * ------------------
 * Stop the worker thread, drain and free any queued jobs, and release
 * scheduler resources. Safe to call multiple times.
 */
void scheduler_shutdown(void) {
    pthread_mutex_lock(&g_mtx);
    if (g_running) {
        g_running = 0;
        pthread_cond_broadcast(&g_cv);
        pthread_mutex_unlock(&g_mtx);
        pthread_join(g_worker, NULL);
    } else {
        pthread_mutex_unlock(&g_mtx);
    }

    // drain heap and free buffers
    pthread_mutex_lock(&g_mtx);
    for (size_t i = 0; i < g_heap.size; ++i) {
        free_job(&g_heap.data[i]);
    }
    free(g_heap.data);
    g_heap.data = NULL;
    g_heap.size = g_heap.cap = 0;
    pthread_mutex_unlock(&g_mtx);

    log_line("Scheduler: worker thread stopped");
}

/*
 * worker_main
 * -----------
 * Main loop for the scheduler worker thread: wait for jobs, process
 * them using the image processing pipeline and free job buffers.
 */
static void* worker_main(void* arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&g_mtx);
        while (g_running && g_heap.size == 0) {
            pthread_cond_wait(&g_cv, &g_mtx);
        }
        if (!g_running && g_heap.size == 0) {
            pthread_mutex_unlock(&g_mtx);
            break;
        }
        ProcJob job;
        int ok = heap_pop_min(&g_heap, &job);
        pthread_mutex_unlock(&g_mtx);
        if (!ok) continue;

        log_line("Scheduler: processing id=%s size=%u file=%s fmt=%s",
                 job.image_id, job.total_size, job.filename, job.format);

        // Procesar desde memoria
        process_image_from_memory(job.data, job.size,
                                  job.image_id, job.filename, job.format,
                                  job.processing_type);

        // liberar buffer del trabajo
        free_job(&job);
        log_line("Scheduler: done id=%s", job.image_id);
    }
    return NULL;
}

// ---------------- Heap helpers ----------------
static int heap_reserve(JobHeap* h, size_t need) {
    if (h->cap >= need) return 0;
    size_t newcap = (h->cap == 0) ? 16 : (h->cap * 2);
    while (newcap < need) newcap *= 2;
    ProcJob* nd = (ProcJob*)realloc(h->data, newcap * sizeof(ProcJob));
    if (!nd) return -1;
    h->data = nd;
    h->cap  = newcap;
    return 0;
}

static void heap_swap(ProcJob* a, ProcJob* b) { ProcJob t = *a; *a = *b; *b = t; }

static int cmp_job(const ProcJob* a, const ProcJob* b) {
    if (a->total_size < b->total_size) return -1;
    if (a->total_size > b->total_size) return 1;
    return strcmp(a->filename, b->filename);
}

static void heap_sift_up(JobHeap* h, size_t idx) {
    while (idx > 0) {
        size_t p = (idx - 1) / 2;
        if (cmp_job(&h->data[idx], &h->data[p]) < 0) {
            heap_swap(&h->data[idx], &h->data[p]);
            idx = p;
        } else break;
    }
}

static void heap_sift_down(JobHeap* h, size_t idx) {
    for (;;) {
        size_t l = idx * 2 + 1, r = l + 1, s = idx;
        if (l < h->size && cmp_job(&h->data[l], &h->data[s]) < 0) s = l;
        if (r < h->size && cmp_job(&h->data[r], &h->data[s]) < 0) s = r;
        if (s != idx) { heap_swap(&h->data[idx], &h->data[s]); idx = s; }
        else break;
    }
}

static int heap_push(JobHeap* h, const ProcJob* j) {
    if (heap_reserve(h, h->size + 1) != 0) return -1;
    h->data[h->size] = *j; // copia superficial; el buffer "data" ya es propiedad del scheduler
    heap_sift_up(h, h->size);
    h->size += 1;
    return 0;
}

static int heap_pop_min(JobHeap* h, ProcJob* out) {
    if (h->size == 0) return 0;
    *out = h->data[0];
    h->size -= 1;
    if (h->size > 0) {
        h->data[0] = h->data[h->size];
        heap_sift_down(h, 0);
    }
    return 1;
}

static void free_job(ProcJob* j) {
    free(j->data);
    j->data = NULL;
    j->size = 0;
}