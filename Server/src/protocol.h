/**
 * protocol.h
 * Protocol definitions for the server
 * (Same as client but in server directory)
 */

#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define DEFAULT_PORT 1717
#define CHUNK_SIZE 4096
#define MAX_FILENAME 256

// Message types
typedef enum {
    MSG_HELLO = 1,
    MSG_IMAGE_ID_REQUEST,
    MSG_IMAGE_ID_RESPONSE,
    MSG_IMAGE_INFO,
    MSG_IMAGE_CHUNK,
    MSG_IMAGE_COMPLETE,
    MSG_ACK,
    MSG_ERROR
} MessageType;

// Processing types
typedef enum {
    PROC_HISTOGRAM = 1,
    PROC_COLOR_CLASSIFICATION = 2,
    PROC_BOTH = 3
} ProcessingType;

// Message header
typedef struct {
    uint8_t type;
    uint32_t length;
    char image_id[37];
} MessageHeader;

// Image info
typedef struct {
    char filename[MAX_FILENAME];
    uint32_t total_size;
    uint32_t total_chunks;
    uint8_t processing_type;
    char format[10];
} ImageInfo;

#endif