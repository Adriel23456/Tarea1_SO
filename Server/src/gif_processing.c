#include "gif_processing.h"
#include "image_processing.h"
#include "config.h"
#include "logging.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "protocol.h"

// Include STB for GIF loading
#include "stb_image.h"

// Include gif.h for writing animated GIFs
#include "gif.h"

// External access to global config
extern ServerConfig g_cfg;

unsigned char* to_rgba(const unsigned char* src, int w, int h, int comp) {
    size_t n = (size_t)w * h;
    unsigned char* out = (unsigned char*)malloc(n * 4);
    if (!out) return NULL;
    
    for (size_t i = 0; i < n; ++i) {
        unsigned char r = 0, g = 0, b = 0, a = 255;
        
        if (comp == 1) { 
            // Grayscale
            r = g = b = src[i]; 
        } else if (comp >= 3) {
            // RGB or RGBA
            r = src[i*comp + 0];
            g = src[i*comp + 1];
            b = src[i*comp + 2];
            if (comp == 4) 
                a = src[i*4 + 3];
        }
        
        out[i*4 + 0] = r;
        out[i*4 + 1] = g;
        out[i*4 + 2] = b;
        out[i*4 + 3] = a;
    }
    
    return out;
}

int write_gif_animation(const char* path, unsigned char** frames_rgba,
                       const int* delays_in, int frame_count, 
                       int w, int h) {
    GifWriter writer = {0};
    
    if (!GifBegin(&writer, path, w, h, 0xFFFF, 8, false)) {
        return 0;
    }

    // Heuristic: detect if delays are in ms and convert to centiseconds
    // Rule: if any delay >= 20 and is multiple of 10, assume milliseconds
    int assume_ms = 0;
    if (delays_in) {
        for (int i = 0; i < frame_count; ++i) {
            int d = delays_in[i];
            if (d >= 20 && (d % 10) == 0) { 
                assume_ms = 1; 
                break; 
            }
        }
    }

    for (int i = 0; i < frame_count; ++i) {
        int d_in = delays_in ? delays_in[i] : 50; // Default 50ms
        
        // Normalize to centiseconds
        int d_cs = assume_ms ? (d_in + 5) / 10 : d_in; // Round up if in ms
        
        // Apply common viewer/browser clamps: minimum 2cs (20ms)
        if (d_cs < 2) d_cs = 2;
        
        // Maximum reasonable value: 50 seconds per frame
        if (d_cs > 5000) d_cs = 5000;

        if (!GifWriteFrame(&writer, frames_rgba[i], w, h, (uint32_t)d_cs, 8, false)) {
            GifEnd(&writer);
            return 0;
        }
    }

    GifEnd(&writer);
    return 1;
}

void process_gif_image(const char* input_path, const char* image_id, 
                      const char* filename, ProcessingType processing_type) {
    // Read file into memory
    int len = 0;
    unsigned char* filebuf = read_file_fully(input_path, &len);
    if (!filebuf) {
        log_line("GIF: cannot read file into memory: %s", input_path);
        return;
    }

    // Load all frames
    int w = 0, h = 0, frames = 0, comp = 0;
    int* delays = NULL; // In centiseconds
    unsigned char* all = stbi_load_gif_from_memory(filebuf, len, &delays, 
                                                   &w, &h, &frames, &comp, 
                                                   4 /* req_comp RGBA */);
    free(filebuf);

    if (!all || frames <= 0 || w <= 0 || h <= 0) {
        log_line("GIF: failed to decode frames: %s", input_path);
        if (all) stbi_image_free(all);
        if (delays) free(delays);
        return;
    }

    // Each frame is w*h*4 bytes (RGBA)
    int rgba_comp = 4;
    size_t frame_stride = (size_t)w * h * rgba_comp;

    // Color classification (copy entire animation)
    if (processing_type == PROC_COLOR_CLASSIFICATION || processing_type == PROC_BOTH) {
        // Calculate dominant color across ALL frames
        unsigned long long r_sum = 0, g_sum = 0, b_sum = 0;
        size_t pixcount = (size_t)w * h;
        
        for (int f = 0; f < frames; ++f) {
            unsigned char* frame = all + f * frame_stride;
            for (size_t i = 0; i < pixcount; ++i) {
                r_sum += frame[i*4 + 0];
                g_sum += frame[i*4 + 1];
                b_sum += frame[i*4 + 2];
            }
        }
        
        // Determine dominant color
        const char* color_dir = g_cfg.colors_red;
        const char* cname = "red";
        
        if (g_sum >= r_sum && g_sum >= b_sum) { 
            color_dir = g_cfg.colors_green; 
            cname = "green"; 
        } else if (b_sum >= r_sum && b_sum >= g_sum) { 
            color_dir = g_cfg.colors_blue; 
            cname = "blue"; 
        }

        // Build output path
        char out_path[1024];
        const char* ext = ".gif";
        snprintf(out_path, sizeof(out_path), "%s/%s_%s%s",
                 color_dir, image_id, filename, 
                 (strstr(filename, ".gif") || strstr(filename, ".GIF")) ? "" : ext);

        // Create frame pointer array for gif.h
        unsigned char** frame_ptrs = (unsigned char**)malloc(sizeof(unsigned char*) * frames);
        if (!frame_ptrs) {
            log_line("GIF: OOM frame_ptrs (classification)");
        } else {
            for (int f = 0; f < frames; ++f) {
                frame_ptrs[f] = all + f * frame_stride;
            }
            
            if (write_gif_animation(out_path, frame_ptrs, delays, frames, w, h)) {
                log_line("Color classification GIF: saved to %s (dominant %s)", 
                         out_path, cname);
            } else {
                log_line("Color classification GIF: failed to write %s", out_path);
            }
            
            free(frame_ptrs);
        }
    }

    // Histogram equalization per frame
    if (processing_type == PROC_HISTOGRAM || processing_type == PROC_BOTH) {
        // Copy frames, equalize RGB, and write animation
        unsigned char** out_frames = (unsigned char**)malloc(sizeof(unsigned char*) * frames);
        
        if (!out_frames) {
            log_line("GIF: OOM out_frames (histogram)");
        } else {
            for (int f = 0; f < frames; ++f) {
                unsigned char* src = all + f * frame_stride;
                unsigned char* dst = (unsigned char*)malloc(frame_stride);
                
                if (!dst) { 
                    out_frames[f] = NULL; 
                    continue; 
                }
                
                memcpy(dst, src, frame_stride);
                
                // Equalize only RGB channels; preserve alpha
                apply_histogram_equalization(dst, w, h, 4);
                out_frames[f] = dst;
            }

            // Build output path
            char out_path[1024];
            const char* ext = ".gif";
            snprintf(out_path, sizeof(out_path), "%s/%s_%s%s",
                     g_cfg.histogram_dir, image_id, filename, 
                     (strstr(filename, ".gif") || strstr(filename, ".GIF")) ? "" : ext);

            int ok = write_gif_animation(out_path, out_frames, delays, frames, w, h);
            
            if (ok) {
                log_line("Histogram equalization GIF: saved to %s", out_path);
            } else {
                log_line("Histogram equalization GIF: failed to write %s", out_path);
            }

            // Free allocated frames
            for (int f = 0; f < frames; ++f) {
                free(out_frames[f]);
            }
            free(out_frames);
        }
    }

    stbi_image_free(all);
    if (delays) free(delays);
}