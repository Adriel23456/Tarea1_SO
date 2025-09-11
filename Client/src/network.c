// Implementación real TCP (con soporte TLS opcional para cliente)
// Compilar con: -luuid -lssl -lcrypto  (TLS opcional)

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include "network.h"
#include "protocol.h"

// ----- TLS (cliente) opcional -----
#include <openssl/ssl.h>
#include <openssl/err.h>

typedef struct {
    int   fd;
    SSL*  ssl;   // NULL si no TLS
} NetStream;

// Utiles
static uint32_t to_be32(uint32_t v) { return htonl(v); }
static uint32_t from_be32(uint32_t v) { return ntohl(v); }

// Envío/recepción (abstracto, maneja fd o SSL)
static ssize_t ns_send(NetStream* ns, const void* buf, size_t len) {
    if (ns->ssl) return SSL_write(ns->ssl, buf, (int)len);
    return send(ns->fd, buf, len, 0);
}
static ssize_t ns_recv(NetStream* ns, void* buf, size_t len) {
    if (ns->ssl) return SSL_read(ns->ssl, buf, (int)len);
    return recv(ns->fd, buf, len, 0);
}

static int send_all(NetStream* ns, const void* buf, size_t len) {
    const uint8_t* p = (const uint8_t*)buf;
    size_t sent = 0;
    while (sent < len) {
        ssize_t n = ns_send(ns, p + sent, len - sent);
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}
static int recv_all(NetStream* ns, void* buf, size_t len) {
    uint8_t* p = (uint8_t*)buf;
    size_t recvd = 0;
    while (recvd < len) {
        ssize_t n = ns_recv(ns, p + recvd, len - recvd);
        if (n <= 0) return -1;
        recvd += (size_t)n;
    }
    return 0;
}

static int send_header(NetStream* ns, uint8_t type, uint32_t payload_len, const char* image_id) {
    MessageHeader h;
    memset(&h, 0, sizeof(h));
    h.type = type;
    h.length = to_be32(payload_len);
    if (image_id) {
        strncpy(h.image_id, image_id, sizeof(h.image_id)-1);
        h.image_id[sizeof(h.image_id)-1] = '\0';
    } else {
        h.image_id[0] = '\0';
    }
    return send_all(ns, &h, sizeof(h));
}
static int recv_header(NetStream* ns, MessageHeader* out) {
    if (recv_all(ns, out, sizeof(*out)) != 0) return -1;
    out->length = from_be32(out->length);
    out->image_id[36] = '\0';
    return 0;
}

static int send_message(NetStream* ns, uint8_t type, const char* image_id,
                        const void* payload, uint32_t payload_len) {
    if (send_header(ns, type, payload_len, image_id) != 0) return -1;
    if (payload_len > 0 && payload) {
        if (send_all(ns, payload, payload_len) != 0) return -1;
    }
    return 0;
}

static int connect_with_retry(const char* host, int port, int timeout_sec,
                              int max_retries, int backoff_ms, NetStream* out_ns,
                              gboolean use_tls) {
    memset(out_ns, 0, sizeof(*out_ns));
    out_ns->fd = -1;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", port);

    for (int attempt = 0; attempt <= max_retries; ++attempt) {
        struct addrinfo hints, *res = NULL, *rp = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        int err = getaddrinfo(host, port_str, &hints, &res);
        if (err != 0) return -1;

        int fd = -1;
        for (rp = res; rp != NULL; rp = rp->ai_next) {
            fd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (fd == -1) continue;

            // timeout de conexión
            struct timeval tv;
            tv.tv_sec = timeout_sec;
            tv.tv_usec = 0;
            setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
            setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

            if (connect(fd, rp->ai_addr, rp->ai_addrlen) == 0) {
                out_ns->fd = fd;
                break;
            } else {
                close(fd);
                fd = -1;
            }
        }
        freeaddrinfo(res);

        if (fd != -1) {
            // TLS si aplica
            if (use_tls) {
                SSL_library_init();
                SSL_load_error_strings();
                OpenSSL_add_all_algorithms();

                SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
                if (!ctx) { close(fd); return -1; }

                SSL* ssl = SSL_new(ctx);
                SSL_CTX_free(ctx); // ctx refcount se mantiene por ssl
                if (!ssl) { close(fd); return -1; }

                SSL_set_fd(ssl, fd);
                if (SSL_connect(ssl) != 1) {
                    SSL_free(ssl);
                    close(fd);
                    return -1;
                }
                out_ns->ssl = ssl;
            }
            return 0; // conectado
        }

        if (attempt < max_retries) {
            if (backoff_ms > 0) g_usleep((gulong)backoff_ms * 1000);
        }
    }
    return -1;
}

static void close_stream(NetStream* ns) {
    if (!ns) return;
    if (ns->ssl) {
        SSL_shutdown(ns->ssl);
        SSL_free(ns->ssl);
        ns->ssl = NULL;
    }
    if (ns->fd != -1) {
        close(ns->fd);
        ns->fd = -1;
    }
}

static const char* ext_from_filename(const char* path) {
    const char* dot = strrchr(path, '.');
    if (!dot || dot == path) return "bin";
    const char* ext = dot + 1;
    if (!strcasecmp(ext, "jpg"))  return "jpg";
    if (!strcasecmp(ext, "jpeg")) return "jpeg";
    if (!strcasecmp(ext, "png"))  return "png";
    if (!strcasecmp(ext, "gif"))  return "gif";
    return "bin";
}

static const char* base_from_path(const char* path) {
    const char* slash = strrchr(path, '/');
#ifdef _WIN32
    const char* bslash = strrchr(path, '\\');
    if (!slash || (bslash && bslash > slash)) slash = bslash;
#endif
    return slash ? slash + 1 : path;
}

static int send_one_image(const char* filepath,
                          const NetConfig* cfg,
                          ProcessingType proc_type,
                          ProgressCallback cb) {
    // 1) Conectar
    if (cb) cb("Connecting to server...", 0.0);
    gboolean want_tls = (cfg->protocol && !g_ascii_strcasecmp(cfg->protocol, "https")==0);
    NetStream ns;
    if (connect_with_retry(cfg->host, cfg->port,
                           cfg->connect_timeout,
                           cfg->max_retries,
                           cfg->retry_backoff_ms,
                           &ns, want_tls) != 0) {
        if (cb) cb("Connection failed", 0.0);
        return -1;
    }

    // 2) Exchange simple para validar vida del server: HELLO -> IMAGE_ID_RESPONSE
    if (send_message(&ns, MSG_HELLO, NULL, NULL, 0) != 0) {
        if (cb) cb("Failed to send HELLO", 0.0);
        close_stream(&ns);
        return -1;
    }

    MessageHeader hdr;
    if (recv_header(&ns, &hdr) != 0 || hdr.type != MSG_IMAGE_ID_RESPONSE) {
        if (cb) cb("Invalid response to HELLO", 0.0);
        close_stream(&ns);
        return -1;
    }
    char image_id[37];
    strncpy(image_id, hdr.image_id, sizeof(image_id));
    image_id[36] = '\0';

    // 3) Preparar ImageInfo
    FILE* f = fopen(filepath, "rb");
    if (!f) {
        if (cb) cb("Failed to open image file", 0.0);
        close_stream(&ns);
        return -1;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); close_stream(&ns); return -1; }
    long total_size_l = ftell(f);
    if (total_size_l < 0) { fclose(f); close_stream(&ns); return -1; }
    rewind(f);

    int chunk = cfg->chunk_size > 0 ? cfg->chunk_size : DEFAULT_CHUNK_SIZE;
    uint32_t total_chunks = (uint32_t)((total_size_l + chunk - 1) / chunk);

    ImageInfo info;
    memset(&info, 0, sizeof(info));
    const char* base = base_from_path(filepath);
    strncpy(info.filename, base, sizeof(info.filename)-1);
    info.total_size   = to_be32((uint32_t)total_size_l);
    info.total_chunks = to_be32(total_chunks);
    info.processing_type = (uint8_t)proc_type;
    strncpy(info.format, ext_from_filename(filepath), sizeof(info.format)-1);

    if (send_message(&ns, MSG_IMAGE_INFO, image_id, &info, sizeof(info)) != 0) {
        if (cb) cb("Failed to send IMAGE_INFO", 0.0);
        fclose(f);
        close_stream(&ns);
        return -1;
    }

    // 4) Enviar por chunks (TCP ya garantiza orden e integridad)
    unsigned char* buf = (unsigned char*)malloc(chunk);
    if (!buf) { fclose(f); close_stream(&ns); return -1; }

    long sent = 0;
    while (!feof(f)) {
        size_t n = fread(buf, 1, chunk, f);
        if (ferror(f)) {
            if (cb) cb("Read error", 0.0);
            free(buf); fclose(f); close_stream(&ns);
            return -1;
        }
        if (n == 0) break;

        if (send_message(&ns, MSG_IMAGE_CHUNK, image_id, buf, (uint32_t)n) != 0) {
            if (cb) cb("Failed to send CHUNK", 0.0);
            free(buf); fclose(f); close_stream(&ns);
            return -1;
        }
        sent += (long)n;

        if (cb && total_size_l > 0) {
            double prog = (double)sent / (double)total_size_l;
            char msg[256];
            g_snprintf(msg, sizeof(msg), "Sending %s", base);
            cb(msg, prog > 1.0 ? 1.0 : prog);
        }
    }
    free(buf);
    fclose(f);

    // 5) Completar (incluye el formato en el payload)
    const char* fmt = ext_from_filename(filepath);
    if (send_message(&ns, MSG_IMAGE_COMPLETE, image_id, fmt, (uint32_t)(strlen(fmt)+1)) != 0) {
        if (cb) cb("Failed to send IMAGE_COMPLETE", 1.0);
        close_stream(&ns);
        return -1;
    }

    // (Opcional) Podríamos esperar un ACK final del server; por ahora lo dejamos fire-and-forget.
    close_stream(&ns);
    if (cb) {
        char msg[256];
        g_snprintf(msg, sizeof(msg), "Finished %s", base);
        cb(msg, 1.0);
    }
    return 0;
}

int send_all_images(GSList* image_list,
                    const NetConfig* cfg,
                    ProcessingType proc_type,
                    ProgressCallback callback) {
    int overall = 0;
    for (GSList* it = image_list; it != NULL; it = it->next) {
        const char* path = (const char*)it->data;
        if (!path) continue;

        int rc = send_one_image(path, cfg, proc_type, callback);
        if (rc != 0 && overall == 0) overall = rc; // conserva primer error
    }
    return overall;
}