#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define DEFAULT_PORT 1717
#define DEFAULT_CHUNK_SIZE 4096
#define MAX_FILENAME 256

// Tipos de mensajes
typedef enum {
    MSG_HELLO = 1,              // Cliente -> Server
    MSG_IMAGE_ID_REQUEST,       // (no usado, dejamos reservado)
    MSG_IMAGE_ID_RESPONSE,      // Server -> Cliente (uuid en header.image_id)
    MSG_IMAGE_INFO,             // Cliente -> Server (ImageInfo payload)
    MSG_IMAGE_CHUNK,            // Cliente -> Server (bytes crudos)
    MSG_IMAGE_COMPLETE,         // Cliente -> Server (payload = "jpg"/"png"/"jpeg"/"gif")
    MSG_ACK,                    // Opcional (no lo usamos por chunk)
    MSG_ERROR                   // Server -> Cliente (texto)
} MessageType;

// Tipos de procesamiento
typedef enum {
    PROC_HISTOGRAM = 1,
    PROC_COLOR_CLASSIFICATION = 2,
    PROC_BOTH = 3
} ProcessingType;

// Header fijo de todos los mensajes enviados por el cliente y el server.
// Notas:
// - length está en big-endian (network order) para portabilidad.
// - image_id es string UUID (36 chars + '\0'), siempre null-terminated.
typedef struct {
    uint8_t  type;              // MessageType
    uint32_t length;            // longitud del payload (network order)
    char     image_id[37];      // UUID "8-4-4-4-12" + '\0'
} MessageHeader;

// Información inicial de la imagen (payload de MSG_IMAGE_INFO)
typedef struct {
    char     filename[MAX_FILENAME]; // nombre base del archivo
    uint32_t total_size;             // bytes totales, network order
    uint32_t total_chunks;           // network order
    uint8_t  processing_type;        // ProcessingType
    char     format[10];             // "jpg","jpeg","png","gif"
} ImageInfo;

#endif