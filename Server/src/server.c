#include "server.h"
#include "config.h"
#include "logging.h"
#include "connection.h"
#include "image_processing.h"
#include "scheduler.h"
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

/*
 * handle_client
 * -------------
 * Thread entry that implements the server-side protocol for a single
 * client connection. It receives messages, reconstructs image data in
 * memory and enqueues processing jobs. The connection struct is freed
 * before the thread exits.
 */
void* handle_client(void* arg) {
    Conn* c = (Conn*)arg;

    // Client state
    char     current_uuid[37] = {0};
    char     current_filename[MAX_FILENAME] = {0};
    char     current_format[10] = {0};
    uint32_t expected_chunks = 0, received_chunks = 0;
    uint32_t remaining_bytes = 0;
    uint32_t total_size = 0;
    ProcessingType  processing_type = 0;

    // In-memory buffer for the complete incoming image
    unsigned char* img_buf = NULL;
    size_t         img_cap = 0;   // == expected total_size
    size_t         img_off = 0;   // bytes written

    int done = 0;
    while (!done) {
        MessageHeader h;

        int hr = recv_header(c, &h);
    if (hr == -2) { // client EOF
            log_line("Client closed connection (EOF)");
            break;
        } else if (hr != 0) {
            log_line("Connection error while waiting for header");
            break;
        }

        if (h.type == MSG_HELLO) {
            uuid_t uu;
            uuid_generate(uu);
            uuid_unparse_lower(uu, current_uuid);
            log_line("HELLO -> new image id = %s", current_uuid);

            if (send_message(c, MSG_IMAGE_ID_RESPONSE, current_uuid, NULL, 0) != 0) {
                log_line("Failed sending IMAGE_ID_RESPONSE");
                break;
            }

        } else if (h.type == MSG_IMAGE_INFO) {
            if (h.length != sizeof(ImageInfo)) {
                log_line("IMAGE_INFO wrong size %u", h.length);
                break;
            }

            ImageInfo info;
            int irc = cs_recv_all(c, &info, sizeof(info));
            if (irc != 0) {
                log_line("Failed to read IMAGE_INFO payload (rc=%d)", irc);
                break;
            }

            total_size = from_be32_s(info.total_size);
            expected_chunks = from_be32_s(info.total_chunks);
            processing_type = (ProcessingType)info.processing_type;

            // Safe copies
            { size_t n = strnlen(info.filename, sizeof(current_filename)-1);
              memcpy(current_filename, info.filename, n); current_filename[n] = '\0'; }
            { size_t n = strnlen(info.format, sizeof(current_format)-1);
              memcpy(current_format, info.format, n); current_format[n] = '\0'; }

            // Allocate buffer in memory
            img_buf = (unsigned char*)malloc(total_size);
            if (!img_buf) {
                log_line("OOM allocating %u bytes for incoming image", total_size);
                break;
            }
            img_cap = total_size;
            img_off = 0;
            remaining_bytes = total_size;

            log_line("IMAGE_INFO: id=%s file=%s size=%u bytes chunks=%u proc=%u fmt=%s",
                     h.image_id, current_filename, total_size, expected_chunks,
                     (unsigned)processing_type, current_format);

        } else if (h.type == MSG_IMAGE_CHUNK) {
            if (!img_buf) { log_line("CHUNK without open buffer"); break; }

            size_t to_read = h.length;
            unsigned char* tmp = (unsigned char*)malloc(to_read);
            if (!tmp) { log_line("OOM on chunk tmp"); break; }

            int crc = cs_recv_all(c, tmp, to_read);
            if (crc != 0) {
                free(tmp);
                log_line("Failed to read chunk body (rc=%d)", crc);
                break;
            }

            // Copy into the accumulated buffer
            if (img_off + to_read > img_cap) {
                free(tmp);
                log_line("Chunk overflow (img_off=%zu to_read=%zu cap=%zu)", img_off, to_read, img_cap);
                break;
            }
            memcpy(img_buf + img_off, tmp, to_read);
            img_off += to_read;
            free(tmp);

            received_chunks++;
            if (remaining_bytes >= to_read) remaining_bytes -= (uint32_t)to_read;
            else remaining_bytes = 0;

        } else if (h.type == MSG_IMAGE_COMPLETE) {
            char fmt[32] = {0};
            if (h.length > 0 && h.length < sizeof(fmt)) {
                int crc = cs_recv_all(c, fmt, h.length);
                if (crc != 0) {
                    log_line("Failed read COMPLETE fmt (rc=%d)", crc);
                    break;
                }
                fmt[sizeof(fmt)-1] = '\0';
            } else if (h.length > 0) {
                char* tmp = (char*)malloc(h.length);
                if (!tmp) break;
                int crc = cs_recv_all(c, tmp, h.length);
                if (crc != 0) { free(tmp); break; }
                free(tmp);
            }

            const char* final_fmt = fmt[0] ? fmt : current_format;

            log_line("IMAGE_COMPLETE: id=%s file=%s fmt=%s chunks=%u remaining=%u",
                     h.image_id, current_filename, final_fmt,
                     received_chunks, remaining_bytes);

            // Enqueue in-memory job (the buffer ownership transfers to the scheduler)
            if (processing_type > 0 && img_buf && img_off == img_cap) {
                ProcJob job;
                memset(&job, 0, sizeof(job));
                job.data = img_buf;          // transfer ownership
                job.size = img_cap;
                { size_t n = strnlen(h.image_id, sizeof(job.image_id)-1);
                  memcpy(job.image_id, h.image_id, n); job.image_id[n] = '\0'; }
                { size_t n = strnlen(current_filename, sizeof(job.filename)-1);
                  memcpy(job.filename, current_filename, n); job.filename[n] = '\0'; }
                { size_t n = strnlen(final_fmt, sizeof(job.format)-1);
                  memcpy(job.format, final_fmt, n); job.format[n] = '\0'; }
                job.processing_type = processing_type;
                job.total_size      = total_size;

                if (scheduler_enqueue(&job) != 0) {
                    log_line("Scheduler enqueue failed for id=%s", h.image_id);
                    // if enqueue fails, free the buffer here
                    free(img_buf);
                }
                // the scheduler owns the buffer now
                img_buf = NULL; img_cap = img_off = 0;
            } else {
                // If not processing, free buffer if allocated
                free(img_buf);
                img_buf = NULL; img_cap = img_off = 0;
            }

            // Final ACK
            if (send_message(c, MSG_ACK, h.image_id, NULL, 0) != 0) {
                log_line("Failed sending final ACK");
                break;
            }

            // Reset state
            current_uuid[0] = 0;
            current_filename[0] = 0;
            current_format[0] = 0;
            expected_chunks = received_chunks = remaining_bytes = 0;
            total_size = 0;
            processing_type = 0;

            // Single-image-per-connection mode: close cleanly after COMPLETE
            done = 1;

        } else {
            if (h.length > 0) {
                char* tmp = (char*)malloc(h.length);
                if (!tmp) break;
                int crc = cs_recv_all(c, tmp, h.length);
                if (crc != 0) { free(tmp); break; }
                free(tmp);
            }
            log_line("Unknown msg type %u, ignored", (unsigned)h.type);
        }
    }

    // cleanup buffer if the connection ends midway
    if (img_buf) { free(img_buf); img_buf = NULL; }

    conn_close(c);
    free(c);
    log_line("Connection closed");
    return NULL;
}

volatile sig_atomic_t g_terminate = 0;
volatile sig_atomic_t g_reload = 0;
static int g_listen_fd = -1;

/*
 * server_request_shutdown
 * -----------------------
 * Request an orderly shutdown of the server: set a termination flag
 * and shutdown the listening socket to interrupt `accept`.
 */
void server_request_shutdown(void) {
    g_terminate = 1;
    if (g_listen_fd >= 0) {
        shutdown(g_listen_fd, SHUT_RDWR);
        // the listening fd is closed by the main loop
    }
}

/*
 * start_server
 * ------------
 * Start the TCP (or TLS) server: create a listening socket and accept
 * incoming connections. For each accepted connection a detached thread
 * is spawned to handle the client.
 * Returns 0 on clean shutdown, -1 on fatal error during startup.
 */
int start_server(void) {
    // Initialize TLS if enabled
    if (g_cfg.tls_enabled) {
        if (tls_init_ctx(g_cfg.tls_dir) != 0) {
            log_line("TLS enabled in config, but initialization failed. Check certificate and key in %s", g_cfg.tls_dir);
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
    g_listen_fd = srv;

    // Enable address reuse
    int opt = 1;
    (void)setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(g_cfg.port);

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        perror("bind");
        close(srv);
        g_listen_fd = -1;
        return -1;
    }

    // Start listening
    if (listen(srv, BACKLOG) != 0) {
        perror("listen");
        close(srv);
        g_listen_fd = -1;
        return -1;
    }

    log_line("Listening with image processing enabled...");

    // Accept loop - one thread per connection
    for (;;) {
        if (g_terminate) break;

        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        int fd = accept(srv, (struct sockaddr*)&cli, &clilen);

        if (fd < 0) {
            if (g_terminate) break;       // shutting down
            if (errno == EINTR) continue; // signal interrupted accept
            perror("accept");
            continue;
        }

        // Optional: I/O timeouts to avoid permanent blocking
        struct timeval tv = { .tv_sec = 15, .tv_usec = 0 };
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
        setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

        // Log client connection
        char cip[64];
        inet_ntop(AF_INET, &cli.sin_addr, cip, sizeof(cip));
        log_line("Accepted connection from %s:%d", cip, ntohs(cli.sin_port));

    // Check whether a reload was requested (SIGHUP)
        if (g_reload) {
            g_reload = 0;
            log_line("Reload flag observed (SIGHUP)");
            // Here you could re-open config/log if you wanted hot reload.
        }

    // Create connection structure
        Conn* c = (Conn*)calloc(1, sizeof(Conn));
        if (!c) {
            close(fd);
            continue;
        }
        c->fd = fd;
        c->ssl = NULL;

    // Configure TLS if enabled
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

        // Launch a thread to handle the client
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

    close(srv);
    g_listen_fd = -1;
    log_line("Server stop: listen socket closed");
    return 0;
}