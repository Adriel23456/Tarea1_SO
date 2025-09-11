/**
 * image_handler.h
 * Image handling header
 */

#ifndef IMAGE_HANDLER_H
#define IMAGE_HANDLER_H

#include <stdint.h>
#include <stddef.h>

// Initialize storage directories
void init_directories(void);

// Save image to appropriate directory
void save_image(const char* image_id, const char* filename, const char* format,
                uint8_t processing_type, uint8_t* data, size_t size);

#endif