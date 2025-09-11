#include "image_processing.h"
#include "gif_processing.h"
#include "config.h"
#include "logging.h"
#include <string.h>
#include <strings.h>
#include <stdlib.h>
#include "protocol.h"

// STB Image libraries
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image.h"
#include "stb_image_write.h"

// External access to global config
extern ServerConfig g_cfg;

char classify_image_by_color(unsigned char* data, int width, int height, int channels) {
    if (channels < 3) return 'r';
    
    unsigned long long r_sum = 0, g_sum = 0, b_sum = 0;
    int pixel_count = width * height;
    
    for (int i = 0; i < pixel_count; i++) {
        int idx = i * channels;
        r_sum += data[idx + 0];
        g_sum += data[idx + 1];
        b_sum += data[idx + 2];
    }
    
    if (r_sum >= g_sum && r_sum >= b_sum) 
        return 'r';
    else if (g_sum >= r_sum && g_sum >= b_sum) 
        return 'g';
    else 
        return 'b';
}

void apply_histogram_equalization(unsigned char* data, int width, int height, int channels) {
    int pixel_count = width * height;
    
    // Process each color channel separately
    for (int ch = 0; ch < channels && ch < 3; ch++) {
        // Build histogram
        int histogram[256] = {0};
        for (int i = 0; i < pixel_count; i++) {
            histogram[data[i*channels + ch]]++;
        }
        
        // Build cumulative distribution
        int cumulative[256] = {0};
        cumulative[0] = histogram[0];
        for (int i = 1; i < 256; i++) {
            cumulative[i] = cumulative[i-1] + histogram[i];
        }
        
        // Apply equalization
        for (int i = 0; i < pixel_count; i++) {
            int idx = i*channels + ch;
            int old_value = data[idx];
            int new_value = (cumulative[old_value] * 255) / pixel_count;
            data[idx] = (unsigned char)new_value;
        }
    }
}

int save_image(const char* path, unsigned char* data, int width, int height, 
               int channels, const char* format) {
    int result = 0;
    
    if (strcmp(format, "png") == 0) {
        result = stbi_write_png(path, width, height, channels, data, width * channels);
    } else if (strcmp(format, "jpg") == 0 || strcmp(format, "jpeg") == 0) {
        result = stbi_write_jpg(path, width, height, channels, data, 95);
    } else if (strcmp(format, "gif") == 0) {
        // For static GIF (rare case), save as PNG to avoid data loss
        result = stbi_write_png(path, width, height, channels, data, width * channels);
    } else {
        // Default to PNG for unknown formats
        result = stbi_write_png(path, width, height, channels, data, width * channels);
    }
    
    return result;
}

void process_static_image(const char* input_path, const char* image_id, 
                         const char* filename, const char* format, 
                         ProcessingType processing_type) {
    // Load image
    int width, height, channels;
    unsigned char* img_data = stbi_load(input_path, &width, &height, &channels, 0);
    
    if (!img_data) {
        log_line("Failed to load image %s for processing", input_path);
        return;
    }

    log_line("Processing image %s: %dx%d, %d channels, type=%u (static)", 
             image_id, width, height, channels, (unsigned)processing_type);

    // Color classification processing
    if (processing_type == PROC_COLOR_CLASSIFICATION || processing_type == PROC_BOTH) {
        char dominant_color = classify_image_by_color(img_data, width, height, channels);
        
        const char* color_dir = g_cfg.colors_red;
        const char* cname = "red";
        
        if (dominant_color == 'g') { 
            color_dir = g_cfg.colors_green; 
            cname = "green"; 
        } else if (dominant_color == 'b') { 
            color_dir = g_cfg.colors_blue; 
            cname = "blue"; 
        }

        char color_path[1024];
        snprintf(color_path, sizeof(color_path), "%s/%s_%s", 
                 color_dir, image_id, filename);
                 
        if (save_image(color_path, img_data, width, height, channels, format)) {
            log_line("Color classification: saved to %s (dominant: %s)", 
                     color_path, cname);
        } else {
            log_line("Failed to save color-classified image to %s", color_path);
        }
    }

    // Histogram equalization processing
    if (processing_type == PROC_HISTOGRAM || processing_type == PROC_BOTH) {
        size_t img_size = (size_t)width * height * channels;
        unsigned char* hist_data = (unsigned char*)malloc(img_size);
        
        if (hist_data) {
            memcpy(hist_data, img_data, img_size);
            apply_histogram_equalization(hist_data, width, height, channels);

            char hist_path[1024];
            snprintf(hist_path, sizeof(hist_path), "%s/%s_%s", 
                     g_cfg.histogram_dir, image_id, filename);
                     
            if (save_image(hist_path, hist_data, width, height, channels, format)) {
                log_line("Histogram equalization: saved to %s", hist_path);
            } else {
                log_line("Failed to save histogram-equalized image to %s", hist_path);
            }
            
            free(hist_data);
        }
    }

    stbi_image_free(img_data);
}

void process_image(const char* input_path, const char* image_id, 
                  const char* filename, const char* format, 
                  ProcessingType processing_type) {
    // Route GIF images to specialized processor
    if (format && (strcasecmp(format, "gif") == 0)) {
        process_gif_image(input_path, image_id, filename, processing_type);
        return;
    }
    
    // Process static images (PNG/JPG/JPEG)
    process_static_image(input_path, image_id, filename, format, processing_type);
}

void process_image_from_memory(const unsigned char* data, size_t size,
                               const char* image_id, const char* filename,
                               const char* format, ProcessingType processing_type) {
    if (!data || size == 0 || !format) return;

    // GIF: canalizar a pipeline de GIF en memoria
    if (format && (strcasecmp(format, "gif") == 0)) {
        process_gif_image_from_memory(data, (int)size, image_id, filename, processing_type);
        return;
    }

    // PNG/JPG/JPEG: usar stbi_load_from_memory
    int width = 0, height = 0, channels = 0;
    unsigned char* img_data = stbi_load_from_memory(data, (int)size, &width, &height, &channels, 0);
    if (!img_data) {
        log_line("Failed to load image from memory (fmt=%s)", format);
        return;
    }

    log_line("Processing (memory) %s: %dx%d, %d ch, type=%u (static)",
             image_id, width, height, channels, processing_type);

    // Clasificación por color
    if (processing_type == PROC_COLOR_CLASSIFICATION || processing_type == PROC_BOTH) {
        char dominant_color = classify_image_by_color(img_data, width, height, channels);
        const char* color_dir = g_cfg.colors_red;
        const char* cname = "red";
        if (dominant_color == 'g') { color_dir = g_cfg.colors_green; cname = "green"; }
        else if (dominant_color == 'b') { color_dir = g_cfg.colors_blue; cname = "blue"; }

        char color_path[1024];
        snprintf(color_path, sizeof(color_path), "%s/%s_%s",
                 color_dir, image_id, filename);

        if (save_image(color_path, img_data, width, height, channels, format)) {
            log_line("Color classification (memory): saved to %s (dominant: %s)", color_path, cname);
        } else {
            log_line("Failed to save color-classified image to %s", color_path);
        }
    }

    // Ecualización de histograma
    if (processing_type == PROC_HISTOGRAM || processing_type == PROC_BOTH) {
        size_t img_size = (size_t)width * height * channels;
        unsigned char* hist_data = (unsigned char*)malloc(img_size);
        if (hist_data) {
            memcpy(hist_data, img_data, img_size);
            apply_histogram_equalization(hist_data, width, height, channels);

            char hist_path[1024];
            snprintf(hist_path, sizeof(hist_path), "%s/%s_%s",
                     g_cfg.histogram_dir, image_id, filename);

            if (save_image(hist_path, hist_data, width, height, channels, format)) {
                log_line("Histogram equalization (memory): saved to %s", hist_path);
            } else {
                log_line("Failed to save histogram-equalized image to %s", hist_path);
            }
            free(hist_data);
        }
    }

    stbi_image_free(img_data);
}