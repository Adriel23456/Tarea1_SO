#ifndef GIF_PROCESSING_H
#define GIF_PROCESSING_H

#include <stdint.h>
#include "protocol.h"

void process_gif_image(const char* input_path, const char* image_id,
                      const char* filename, ProcessingType processing_type);

// NUEVO: versión que trabaja desde buffer en memoria
void process_gif_image_from_memory(const unsigned char* data, int len,
                                  const char* image_id, const char* filename,
                                  ProcessingType processing_type);

unsigned char* to_rgba(const unsigned char* src, int w, int h, int comp);

int write_gif_animation(const char* path, unsigned char** frames_rgba,
                       const int* delays_in, int frame_count,
                       int w, int h);

#endif // GIF_PROCESSING_H