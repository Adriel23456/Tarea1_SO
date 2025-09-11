#ifndef NETWORK_H
#define NETWORK_H

#include <gtk/gtk.h>
#include "protocol.h"

// Callback de progreso (para la UI)
typedef void (*ProgressCallback)(const char* message, double progress);

// Config leída de assets/connection.json
// Ahora con buffers PROPIOS (no punteros a json-c) para evitar uso de memoria liberada.
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