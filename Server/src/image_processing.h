#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h"

char classify_image_by_color(unsigned char* data, int width, int height, int channels);
void apply_histogram_equalization(unsigned char* data, int width, int height, int channels);
int  save_image(const char* path, unsigned char* data, int width, int height, int channels, const char* format);

// EXISTING (from path)
void process_static_image(const char* input_path, const char* image_id,
                         const char* filename, const char* format,
                         ProcessingType processing_type);
void process_image(const char* input_path, const char* image_id,
                  const char* filename, const char* format,
                  ProcessingType processing_type);

// NEW: process from memory (without writing to incoming directory)
void process_image_from_memory(const unsigned char* data, size_t size,
                               const char* image_id, const char* filename,
                               const char* format, ProcessingType processing_type);

#endif // IMAGE_PROCESSING_H