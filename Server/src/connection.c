#include "connection.h"
#include "utils.h"
#include "logging.h"
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <openssl/err.h>

// Global SSL context
static SSL_CTX* g_ssl_ctx = NULL;

/*
 * tls_init_ctx
 * ------------
 * Initialize a global OpenSSL server context using certificate and key
 * files located in `tls_dir`.
 * Parameters:
 *  - tls_dir: directory containing `server.crt` and `server.key`.
 * Returns:
 *  - 0 on success, -1 on failure.
 */
int tls_init_ctx(const char* tls_dir) {
    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    // Create SSL context
    g_ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!g_ssl_ctx) return -1;

    // Load certificate and key files
    char crt[1024], key[1024];
    snprintf(crt, sizeof(crt), "%s/server.crt", tls_dir);
    snprintf(key, sizeof(key), "%s/server.key", tls_dir);

    if (SSL_CTX_use_certificate_file(g_ssl_ctx, crt, SSL_FILETYPE_PEM) != 1) 
        return -1;
    if (SSL_CTX_use_PrivateKey_file(g_ssl_ctx, key, SSL_FILETYPE_PEM) != 1) 
        return -1;
    if (!SSL_CTX_check_private_key(g_ssl_ctx)) 
        return -1;

    return 0;
}

/*
 * tls_cleanup
 * -----------
 * Free global SSL_CTX and cleanup TLS resources.
 */
void tls_cleanup(void) {
    if (g_ssl_ctx) {
        SSL_CTX_free(g_ssl_ctx);
        g_ssl_ctx = NULL;
    }
}

/*
 * get_ssl_ctx
 * -----------
 * Return the global SSL_CTX previously initialized with tls_init_ctx.
 * Returns NULL if TLS is not initialized.
 */
SSL_CTX* get_ssl_ctx(void) {
    return g_ssl_ctx;
}

/*
 * cs_send_all
 * -----------
 * Send exactly `len` bytes over a connection, using TLS if the
 * connection's SSL pointer is set, otherwise send over the raw socket.
 * Parameters:
 *  - c: connection object (must be non-NULL).
 *  - buf: data buffer to send.
 *  - len: number of bytes to send.
 * Returns:
 *  - 0 on success, -1 on any send error.
 */
int cs_send_all(Conn* c, const void* buf, size_t len) {
    const unsigned char* p = (const unsigned char*)buf;
    size_t s = 0;
    
    while (s < len) {
        ssize_t n = c->ssl 
            ? SSL_write(c->ssl, p + s, (int)(len - s))
            : send(c->fd, p + s, len - s, 0);
            
        if (n <= 0) return -1;
        s += (size_t)n;
    }
    
    return 0;
}

/*
 * cs_recv_all
 * -----------
 * Receive exactly `len` bytes into `buf` from the connection. Uses
 * SSL_read if `c->ssl` is set or `recv` otherwise. Handles short reads.
 * Returns 0 on success, -1 on error, or -2 on orderly EOF (peer closed).
 */
int cs_recv_all(Conn* c, void* buf, size_t len) {
    unsigned char* p = (unsigned char*)buf;
    size_t r = 0;

    while (r < len) {
        ssize_t n = c->ssl
            ? SSL_read(c->ssl, p + r, (int)(len - r))
            : recv(c->fd, p + r, len - r, 0);

        if (n == 0) {
            // orderly close from peer (EOF)
            return -2; // NEW: signal EOF with a distinct return code
        }
        if (n < 0) {
            return -1; // error real de lectura
        }
        r += (size_t)n;
    }

    return 0; // éxito
}

/*
 * send_header
 * -----------
 * Build and send a protocol MessageHeader containing `type`, payload
 * length and optional image id. The header is converted to network
 * byte order where required before sending.
 */
int send_header(Conn* c, uint8_t type, uint32_t payload_len, const char* image_id) {
    MessageHeader h;
    memset(&h, 0, sizeof(h));
    
    h.type = type;
    h.length = to_be32_s(payload_len);
    
    if (image_id) {
        strncpy(h.image_id, image_id, sizeof(h.image_id)-1);
        h.image_id[sizeof(h.image_id)-1] = '\0';
    } else {
        h.image_id[0] = '\0';
    }
    
    return cs_send_all(c, &h, sizeof(h));
}

/*
 * recv_header
 * -----------
 * Read a MessageHeader from the connection and convert fields to host
 * byte order. Returns 0 on success, -1 on error, -2 on EOF.
 */
int recv_header(Conn* c, MessageHeader* out) {
    int rc = cs_recv_all(c, out, sizeof(*out));
    if (rc != 0) {
        // rc == -2 (EOF) o rc == -1 (error)
        return rc;
    }

    out->length = from_be32_s(out->length);
    out->image_id[36] = '\0';

    return 0;
}

/*
 * send_message
 * ------------
 * Convenience helper that sends a header followed by an optional
 * payload. Returns 0 on success, -1 on failure.
 */
int send_message(Conn* c, uint8_t type, const char* image_id,
                const void* payload, uint32_t payload_len) {
    if (send_header(c, type, payload_len, image_id) != 0) 
        return -1;
        
    if (payload_len > 0 && payload) {
        if (cs_send_all(c, payload, payload_len) != 0) 
            return -1;
    }
    
    return 0;
}

/*
 * conn_close
 * ----------
 * Gracefully shutdown and free resources associated with a Conn.
 * - Performs TLS shutdown and frees SSL object if present.
 * - Closes the underlying socket and marks file descriptor invalid.
 */
void conn_close(Conn* c) {
    if (!c) return;
    
    if (c->ssl) { 
        SSL_shutdown(c->ssl); 
        SSL_free(c->ssl); 
        c->ssl = NULL; 
    }
    
    if (c->fd >= 0) { 
        close(c->fd); 
        c->fd = -1; 
    }
}