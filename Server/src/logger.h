/**
 * logger.h
 * Logger header file
 */

#ifndef LOGGER_H
#define LOGGER_H

// Initialize the logger
void init_logger(void);

// Close the logger
void close_logger(void);

// Log an event
void log_event(const char* format, ...);

#endif