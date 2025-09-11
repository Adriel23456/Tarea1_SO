/**
 * logger.c
 * Logging functionality for the server
 */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include "logger.h"

#define LOG_FILE "./server.log"

static FILE* log_file = NULL;
static pthread_mutex_t log_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize logger
void init_logger() {
    pthread_mutex_lock(&log_mutex);
    log_file = fopen(LOG_FILE, "a");
    if (log_file) {
        fprintf(log_file, "\n");
        fprintf(log_file, "========================================\n");
        fprintf(log_file, "    SERVER SESSION STARTED\n");
        fprintf(log_file, "========================================\n");
        fflush(log_file);
    }
    pthread_mutex_unlock(&log_mutex);
}

// Close logger
void close_logger() {
    pthread_mutex_lock(&log_mutex);
    if (log_file) {
        fprintf(log_file, "========================================\n");
        fprintf(log_file, "    SERVER SESSION ENDED\n");
        fprintf(log_file, "========================================\n\n");
        fclose(log_file);
        log_file = NULL;
    }
    pthread_mutex_unlock(&log_mutex);
}

// Log an event
void log_event(const char* format, ...) {
    pthread_mutex_lock(&log_mutex);
    
    // Get timestamp
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    
    // Write to file
    if (log_file) {
        fprintf(log_file, "[%s] ", timestamp);
        
        va_list args;
        va_start(args, format);
        vfprintf(log_file, format, args);
        va_end(args);
        
        fprintf(log_file, "\n");
        fflush(log_file);
    }
    
    // Also print to console
    printf("[%s] ", timestamp);
    
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    
    printf("\n");
    
    pthread_mutex_unlock(&log_mutex);
}