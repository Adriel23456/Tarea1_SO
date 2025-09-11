#ifndef GIF_PROCESSING_H
#define GIF_PROCESSING_H

#include <stdint.h>
#include "protocol.h"

// Process GIF image (animated or static)
void process_gif_image(const char* input_path, const char* image_id,
                       const char* filename, ProcessingType processing_type);

// Convert frame from N channels to RGBA
unsigned char* to_rgba(const unsigned char* src, int w, int h, int comp);

// Write GIF animation using gif.h library
// Returns: 1 on success, 0 on failure
int write_gif_animation(const char* path, unsigned char** frames_rgba,
                       const int* delays_in, int frame_count, 
                       int w, int h);

#endif // GIF_PROCESSING_H