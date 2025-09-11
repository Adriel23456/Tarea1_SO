#ifndef CONNECTION_H
#define CONNECTION_H

#include <stdint.h>
#include <openssl/ssl.h>
#include "protocol.h"

// Connection abstraction for both plain TCP and TLS
typedef struct {
    int  fd;    // Socket file descriptor
    SSL* ssl;   // SSL connection (NULL for plain TCP)
} Conn;

// Initialize TLS context using configuration
// Returns: 0 on success, -1 on failure
int tls_init_ctx(const char* tls_dir);

// Clean up TLS context
void tls_cleanup(void);

// Send all data through connection
// Returns: 0 on success, -1 on failure
int cs_send_all(Conn* c, const void* buf, size_t len);

// Receive all data through connection
// Returns: 0 on success, -1 on failure
int cs_recv_all(Conn* c, void* buf, size_t len);

// Send protocol message header
// Returns: 0 on success, -1 on failure
int send_header(Conn* c, uint8_t type, uint32_t payload_len, const char* image_id);

// Receive protocol message header
// Returns: 0 on success, -1 on failure
int recv_header(Conn* c, MessageHeader* out);

// Send complete message (header + payload)
// Returns: 0 on success, -1 on failure
int send_message(Conn* c, uint8_t type, const char* image_id,
                const void* payload, uint32_t payload_len);

// Close connection and free resources
void conn_close(Conn* c);

#endif // CONNECTION_H