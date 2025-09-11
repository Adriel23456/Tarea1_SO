#ifndef NETWORK_H
#define NETWORK_H

#include <gtk/gtk.h>
#include "protocol.h"

// Callback de progreso (para la UI)
typedef void (*ProgressCallback)(const char* message, double progress);

// Config leída de assets/connection.json
typedef struct {
    const char* host;
    int         port;
    const char* protocol;          // "http" o "https" (https requiere server TLS)
    int         chunk_size;        // bytes por chunk
    int         connect_timeout;   // seg
    int         max_retries;       // reintentos para conectar
    int         retry_backoff_ms;  // ms entre reintentos
} NetConfig;

// Envía todas las imágenes secuencialmente (UNA A LA VEZ)
int send_all_images(GSList* image_list,
                    const NetConfig* cfg,
                    ProcessingType proc_type,
                    ProgressCallback callback);

#endif