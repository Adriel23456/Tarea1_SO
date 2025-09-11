/**
 * image_handler.c
 * Image saving and processing functions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include "image_handler.h"
#include "logger.h"

// Base directories
#define BASE_DIR "./received_images"
#define HISTOGRAM_DIR "./received_images/histogram"
#define COLOR_DIR "./received_images/colors"

// Initialize directories
void init_directories() {
    // Create base directories
    mkdir(BASE_DIR, 0755);
    mkdir(HISTOGRAM_DIR, 0755);
    mkdir(COLOR_DIR, 0755);
    
    // Create color subdirectories
    char path[256];
    sprintf(path, "%s/verdes", COLOR_DIR);
    mkdir(path, 0755);
    sprintf(path, "%s/rojas", COLOR_DIR);
    mkdir(path, 0755);
    sprintf(path, "%s/azules", COLOR_DIR);
    mkdir(path, 0755);
    
    log_event("Directories initialized");
}

// Determine dominant color (simple implementation)
const char* get_dominant_color(uint8_t* image_data, size_t size) {
    // For now, just return a random color
    // In a real implementation, you would analyze the image data
    int random = rand() % 3;
    switch(random) {
        case 0: return "rojas";
        case 1: return "verdes";
        case 2: return "azules";
        default: return "verdes";
    }
}

// Save image to disk
void save_image(const char* image_id, const char* filename, const char* format,
                uint8_t processing_type, uint8_t* data, size_t size) {
    char filepath[512];
    char timestamp[32];
    
    // Get timestamp
    time_t now = time(NULL);
    strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", localtime(&now));
    
    // Determine save location based on processing type
    if (processing_type & PROC_HISTOGRAM) {
        // Save to histogram directory
        sprintf(filepath, "%s/%s_%s_%s", HISTOGRAM_DIR, timestamp, image_id, filename);
        
        FILE* fp = fopen(filepath, "wb");
        if (fp) {
            fwrite(data, 1, size, fp);
            fclose(fp);
            log_event("Saved histogram image: %s (%.2f MB)", filepath, size / (1024.0 * 1024.0));
            printf("Image saved for histogram processing: %s\n", filepath);
        } else {
            log_event("ERROR: Failed to save histogram image: %s", filepath);
        }
    }
    
    if (processing_type & PROC_COLOR_CLASSIFICATION) {
        // Determine dominant color and save to appropriate directory
        const char* color = get_dominant_color(data, size);
        sprintf(filepath, "%s/%s/%s_%s_%s", COLOR_DIR, color, timestamp, image_id, filename);
        
        FILE* fp = fopen(filepath, "wb");
        if (fp) {
            fwrite(data, 1, size, fp);
            fclose(fp);
            log_event("Saved color classification image to %s: %s (%.2f MB)", 
                     color, filepath, size / (1024.0 * 1024.0));
            printf("Image saved for color classification (%s): %s\n", color, filepath);
        } else {
            log_event("ERROR: Failed to save color classification image: %s", filepath);
        }
    }
    
    // If neither processing type is specified, save to base directory
    if (!(processing_type & (PROC_HISTOGRAM | PROC_COLOR_CLASSIFICATION))) {
        sprintf(filepath, "%s/%s_%s_%s", BASE_DIR, timestamp, image_id, filename);
        
        FILE* fp = fopen(filepath, "wb");
        if (fp) {
            fwrite(data, 1, size, fp);
            fclose(fp);
            log_event("Saved image to base directory: %s (%.2f MB)", 
                     filepath, size / (1024.0 * 1024.0));
            printf("Image saved: %s\n", filepath);
        } else {
            log_event("ERROR: Failed to save image: %s", filepath);
        }
    }
}