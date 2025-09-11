#ifndef IMAGE_PROCESSING_H
#define IMAGE_PROCESSING_H

#include <stdint.h>
#include "protocol.h"

// Determine dominant color channel (R, G, or B)
// Returns: 'r', 'g', or 'b'
char classify_image_by_color(unsigned char* data, int width, int height, int channels);

// Apply histogram equalization to improve contrast
void apply_histogram_equalization(unsigned char* data, int width, int height, int channels);

// Save image to file in specified format
// Returns: 1 on success, 0 on failure
int save_image(const char* path, unsigned char* data, int width, int height, 
               int channels, const char* format);

// Process static image (non-GIF) based on processing type
void process_static_image(const char* input_path, const char* image_id,
                          const char* filename, const char* format,
                          ProcessingType processing_type);

// Main image processing entry point
void process_image(const char* input_path, const char* image_id,
                   const char* filename, const char* format,
                   ProcessingType processing_type);

#endif // IMAGE_PROCESSING_H