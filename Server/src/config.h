#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

// Server configuration structure
typedef struct {
    int   port;
    int   tls_enabled;              // 1 = enabled, 0 = disabled
    char  tls_dir[512];            // Directory for TLS certificates
    char  log_file[512];           // Path to log file
    char  incoming_dir[512];       // Directory for incoming images
    char  histogram_dir[512];      // Directory for histogram processed images
    char  colors_red[512];         // Directory for red-dominant images
    char  colors_green[512];       // Directory for green-dominant images
    char  colors_blue[512];        // Directory for blue-dominant images
} ServerConfig;

// Set default configuration values
void set_default_config(ServerConfig* c);

// Load configuration from JSON file
// Returns: 0 on success, -1 on failure
int load_config_json(const char* path, ServerConfig* c);

// Create all required directories from configuration
// Returns: 0 on success, -1 on failure
int ensure_dirs_from_config(const ServerConfig* c);

#endif // CONFIG_H