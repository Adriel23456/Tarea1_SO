#include "server.h"
#include "config.h"
#include "logging.h"
#include "connection.h"
#include "image_processing.h"
#include "utils.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <uuid/uuid.h>

#define BACKLOG 10

// Global configuration
extern ServerConfig g_cfg;
extern SSL_CTX* get_ssl_ctx(void);

void* handle_client(void* arg) {
    Conn* c = (Conn*)arg;

    // Client state variables
    char     current_uuid[37] = {0};
    char     current_filename[MAX_FILENAME] = {0};
    char     current_format[10] = {0};
    FILE*    out = NULL;
    uint32_t expected_chunks = 0, received_chunks = 0;
    uint32_t remaining_bytes = 0;
    ProcessingType  processing_type = 0;  // tipado fuerte
    char     saved_path[1024] = {0};

    int done = 0;
    while (!done) {
        MessageHeader h;

        // Receive message header
        if (recv_header(c, &h) != 0) {
            log_line("Connection dropped while waiting header");
            break;
        }

        // Process message based on type
        if (h.type == MSG_HELLO) {
            // Generate new UUID for this image
            uuid_t uu;
            uuid_generate(uu);
            uuid_unparse_lower(uu, current_uuid);
            log_line("HELLO -> new image id = %s", current_uuid);

            // Send UUID response
            if (send_message(c, MSG_IMAGE_ID_RESPONSE, current_uuid, NULL, 0) != 0) {
                log_line("Failed sending IMAGE_ID_RESPONSE");
                break;
            }

        } else if (h.type == MSG_IMAGE_INFO) {
            // Validate payload size
            if (h.length != sizeof(ImageInfo)) {
                log_line("IMAGE_INFO wrong size %u", h.length);
                break;
            }

            // Receive image info
            ImageInfo info;
            if (cs_recv_all(c, &info, sizeof(info)) != 0) {
                log_line("Failed to read IMAGE_INFO payload");
                break;
            }

            // Parse image info
            uint32_t total_size = from_be32_s(info.total_size);
            expected_chunks = from_be32_s(info.total_chunks);
            processing_type = (ProcessingType)info.processing_type;

            // current_filename (dest 256, src 256 en el struct)
            {
                size_t n = strnlen(info.filename, sizeof(current_filename) - 1);
                memcpy(current_filename, info.filename, n);
                current_filename[n] = '\0';
            }
            // current_format (dest 10, src 10 en el struct)
            {
                size_t n = strnlen(info.format, sizeof(current_format) - 1);
                memcpy(current_format, info.format, n);
                current_format[n] = '\0';
            }

            // Open output file in incoming directory
            snprintf(saved_path, sizeof(saved_path), "%s/%s_%s",
                     g_cfg.incoming_dir, h.image_id, current_filename);

            if (ensure_parent_dir(saved_path) != 0) {
                log_line("Failed to create parent dir for %s", saved_path);
                break;
            }

            out = fopen(saved_path, "wb");
            if (!out) {
                log_line("Failed to open output file %s: %s", saved_path, strerror(errno));
                break;
            }

            remaining_bytes = total_size;

            log_line("IMAGE_INFO: id=%s file=%s size=%u bytes chunks=%u proc=%u fmt=%s",
                h.image_id, current_filename, total_size, expected_chunks,
                (unsigned)processing_type, current_format);

        } else if (h.type == MSG_IMAGE_CHUNK) {
            // Validate file is open
            if (!out) {
                log_line("CHUNK without open file");
                break;
            }

            // Receive chunk data
            size_t to_read = h.length;
            unsigned char* buf = (unsigned char*)malloc(to_read);
            if (!buf) {
                log_line("OOM on chunk");
                break;
            }

            if (cs_recv_all(c, buf, to_read) != 0) {
                free(buf);
                log_line("Failed to read chunk body");
                break;
            }

            // Write chunk to file
            size_t w = fwrite(buf, 1, to_read, out);
            free(buf);

            if (w != to_read) {
                log_line("Write error");
                break;
            }

            received_chunks++;
            if (remaining_bytes >= to_read)
                remaining_bytes -= (uint32_t)to_read;
            else
                remaining_bytes = 0;

        } else if (h.type == MSG_IMAGE_COMPLETE) {
            // Receive format string if present
            char fmt[32] = {0};
            if (h.length > 0 && h.length < sizeof(fmt)) {
                if (cs_recv_all(c, fmt, h.length) != 0) {
                    log_line("Failed read COMPLETE fmt");
                    break;
                }
                fmt[sizeof(fmt) - 1] = '\0';
            } else if (h.length > 0) {
                // Consume unexpected payload
                char* tmp = (char*)malloc(h.length);
                if (!tmp) break;
                if (cs_recv_all(c, tmp, h.length) != 0) {
                    free(tmp);
                    break;
                }
                free(tmp);
            }

            // Close output file
            if (out) {
                fclose(out);
                out = NULL;
            }

            log_line("IMAGE_COMPLETE: id=%s file=%s fmt=%s chunks=%u remaining=%u",
                     h.image_id, current_filename, fmt[0] ? fmt : current_format,
                     received_chunks, remaining_bytes);

            // Process the image if processing type is specified
            if (processing_type > 0) {
                process_image(saved_path, h.image_id, current_filename,
                              fmt[0] ? fmt : current_format, processing_type);
            }

            // Send final acknowledgment
            if (send_message(c, MSG_ACK, h.image_id, NULL, 0) != 0) {
                log_line("Failed sending final ACK");
                break;
            }

            // Reset state for next image
            current_uuid[0] = 0;
            current_filename[0] = 0;
            current_format[0] = 0;
            saved_path[0] = 0;
            expected_chunks = received_chunks = remaining_bytes = 0;
            processing_type = 0;

        } else {
            // Unknown message type - consume payload and ignore
            if (h.length > 0) {
                char* tmp = (char*)malloc(h.length);
                if (!tmp) break;
                if (cs_recv_all(c, tmp, h.length) != 0) {
                    free(tmp);
                    break;
                }
                free(tmp);
            }
            log_line("Unknown msg type %u, ignored", (unsigned)h.type);
        }
    }

    // Cleanup
    if (out) {
        fclose(out);
        out = NULL;
    }

    conn_close(c);
    free(c);
    log_line("Connection closed");

    return NULL;
}

int start_server(void) {
    // Initialize TLS if enabled
    if (g_cfg.tls_enabled) {
        if (tls_init_ctx(g_cfg.tls_dir) != 0) {
            log_line("TLS enabled in config, but initialization failed. "
                    "Check certificate and key in %s", g_cfg.tls_dir);
            return -1;
        }
        log_line("TLS enabled (listening TLS) on port %d", g_cfg.port);
    } else {
        log_line("Server starting (plain TCP) on port %d", g_cfg.port);
    }

    // Create listening socket
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { 
        perror("socket"); 
        return -1; 
    }

    // Enable address reuse
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(g_cfg.port);

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        perror("bind");
        close(srv);
        return -1;
    }
    
    // Start listening
    if (listen(srv, BACKLOG) != 0) {
        perror("listen");
        close(srv);
        return -1;
    }
    
    log_line("Listening with image processing enabled...");

    // Accept loop - one thread per connection
    for (;;) {
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        int fd = accept(srv, (struct sockaddr*)&cli, &clilen);
        
        if (fd < 0) { 
            perror("accept"); 
            continue; 
        }

        // Log client connection
        char cip[64];
        inet_ntop(AF_INET, &cli.sin_addr, cip, sizeof(cip));
        log_line("Accepted connection from %s:%d", cip, ntohs(cli.sin_port));

        // Create connection structure
        Conn* c = (Conn*)calloc(1, sizeof(Conn));
        if (!c) { 
            close(fd); 
            continue; 
        }
        
        c->fd = fd;
        c->ssl = NULL;

        // Setup TLS if enabled
        if (g_cfg.tls_enabled) {
            SSL* ssl = SSL_new(get_ssl_ctx());
            if (!ssl) {
                close(fd);
                free(c);
                continue;
            }
            
            SSL_set_fd(ssl, fd);
            
            if (SSL_accept(ssl) != 1) {
                log_line("TLS handshake failed");
                SSL_free(ssl);
                close(fd);
                free(c);
                continue;
            }
            
            c->ssl = ssl;
        }

        // Create thread to handle client
        pthread_t th;
        int rc = pthread_create(&th, NULL, handle_client, c);
        
        if (rc != 0) {
            log_line("pthread_create failed");
            conn_close(c);
            free(c);
            continue;
        }
        
        pthread_detach(th);
    }

    // Cleanup (unreachable in normal operation)
    close(srv);
    return 0;
}