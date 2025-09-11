#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>

typedef struct {
    int   port;
    int   tls_enabled;              // 1 = enabled, 0 = disabled
    char  tls_dir[512];             // Directory for TLS certificates
    char  log_file[512];            // Path to log file
    char  histogram_dir[512];       // Directory for histogram processed images
    char  colors_red[512];          // Directory for red-dominant images
    char  colors_green[512];        // Directory for green-dominant images
    char  colors_blue[512];         // Directory for blue-dominant images
} ServerConfig;

void set_default_config(ServerConfig* c);
int load_config_json(const char* path, ServerConfig* c);
int ensure_dirs_from_config(const ServerConfig* c);

#endif // CONFIG_H