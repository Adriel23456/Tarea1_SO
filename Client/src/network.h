#ifndef NETWORK_H
#define NETWORK_H

#include <gtk/gtk.h>
#include "protocol.h"

// Progress callback (for the UI)
typedef void (*ProgressCallback)(const char* message, double progress);

// Configuration read from assets/connection.json
// Uses its own buffers (not pointers into json-c objects) to avoid
// use-after-free when the JSON object is released.
typedef struct {
    char host[256];
    int  port;
    char protocol[16];         // "http" o "https"
    int  chunk_size;           // bytes por chunk
    int  connect_timeout;      // seg
    int  max_retries;          // reintentos para conectar
    int  retry_backoff_ms;     // ms
} NetConfig;

// Envía todas las imágenes secuencialmente (UNA A LA VEZ)
int send_all_images(GSList* image_list,
                    const NetConfig* cfg,
                    ProcessingType proc_type,
                    ProgressCallback callback);

#endif