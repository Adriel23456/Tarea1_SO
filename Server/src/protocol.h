#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define DEFAULT_PORT 1717
#define DEFAULT_CHUNK_SIZE 4096
#define MAX_FILENAME 256

// Message types
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

// Processing types
typedef enum {
    PROC_HISTOGRAM = 1,
    PROC_COLOR_CLASSIFICATION = 2,
    PROC_BOTH = 3
} ProcessingType;

// Fixed header for all messages exchanged between client and server.
// Notes:
// - length is encoded in big-endian (network order) for portability.
// - image_id is a UUID string (36 chars + '\0'), always null-terminated.
typedef struct {
    uint8_t  type;              // MessageType
    uint32_t length;            // longitud del payload (network order)
    char     image_id[37];      // UUID "8-4-4-4-12" + '\0'
} MessageHeader;

// Initial image information (payload of MSG_IMAGE_INFO)
typedef struct {
    char     filename[MAX_FILENAME]; // base filename
    uint32_t total_size;             // total bytes, network order
    uint32_t total_chunks;           // network order
    uint8_t  processing_type;        // ProcessingType
    char     format[10];             // "jpg","jpeg","png","gif"
} ImageInfo;

#endif