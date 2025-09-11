#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>

// Initialize logging system
// Returns: 0 on success, -1 on failure
int log_init(const char* log_file);

// Write a formatted log line with timestamp
void log_line(const char* fmt, ...);

// Close logging system
void log_close(void);

#endif // LOGGING_H