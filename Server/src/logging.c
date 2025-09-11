#include "logging.h"
#include <time.h>
#include <stdarg.h>
#include <pthread.h>

// Global log file handle
static FILE* g_log = NULL;

// Mutex for thread-safe logging
static pthread_mutex_t g_log_mtx = PTHREAD_MUTEX_INITIALIZER;

int log_init(const char* log_file) {
    if (g_log) {
        fclose(g_log);
    }
    
    g_log = fopen(log_file, "a");
    if (!g_log) {
        return -1;
    }
    
    return 0;
}

void log_line(const char* fmt, ...) {
    if (!g_log) return;
    
    pthread_mutex_lock(&g_log_mtx);

    // Get current timestamp
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

    // Write timestamp
    fprintf(g_log, "[%s] ", ts);

    // Write formatted message
    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);

    fprintf(g_log, "\n");
    fflush(g_log);
    
    pthread_mutex_unlock(&g_log_mtx);
}

void log_close(void) {
    if (g_log) {
        fclose(g_log);
        g_log = NULL;
    }
}