// Servidor TCP/TLS concurrente que recibe imágenes por nuestro protocolo.
// Guarda cada imagen en assets/incoming/<uuid>_<filename>
// y escribe eventos en assets/log.txt
//
// Cambios pedidos e implementados en este archivo:
// 1) ACK final (MSG_ACK) después de recibir MSG_IMAGE_COMPLETE
// 2) TLS del lado del servidor mediante OpenSSL (habilitable con --tls o SERVER_TLS=1)
// 3) Concurrencia multi-cliente (un hilo por conexión con pthreads)
//
// Compilar linkeando uuid, pthread y OpenSSL (Makefile ya actualizado):
//   LIBS = -luuid -lssl -lcrypto -lpthread

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <uuid/uuid.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <pthread.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include "protocol.h"

#define LISTEN_PORT DEFAULT_PORT
#define BACKLOG 10

// --- Logging ---
static FILE* g_log = NULL;
static pthread_mutex_t g_log_mtx = PTHREAD_MUTEX_INITIALIZER;

// --- TLS globals ---
static int g_tls_enabled = 0;
static SSL_CTX* g_ssl_ctx = NULL;

// --- Utilidades / FS ---
static void ensure_dirs(void) {
    mkdir("assets", 0755);
    mkdir("assets/incoming", 0755);
    mkdir("assets/tls", 0755); // por si el usuario coloca ahí server.crt y server.key
}

static void log_line(const char* fmt, ...) {
    if (!g_log) return;
    pthread_mutex_lock(&g_log_mtx);

    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    char ts[64];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);

    fprintf(g_log, "[%s] ", ts);

    va_list ap;
    va_start(ap, fmt);
    vfprintf(g_log, fmt, ap);
    va_end(ap);

    fprintf(g_log, "\n");
    fflush(g_log);
    pthread_mutex_unlock(&g_log_mtx);
}

// --- Conexión abstracta (plain o TLS) ---
typedef struct {
    int  fd;
    SSL* ssl; // NULL si no TLS
} Conn;

static int cs_send_all(Conn* c, const void* buf, size_t len) {
    const unsigned char* p = (const unsigned char*)buf;
    size_t s = 0;
    while (s < len) {
        ssize_t n;
        if (c->ssl) n = SSL_write(c->ssl, p + s, (int)(len - s));
        else        n = send(c->fd, p + s, len - s, 0);
        if (n <= 0) return -1;
        s += (size_t)n;
    }
    return 0;
}

static int cs_recv_all(Conn* c, void* buf, size_t len) {
    unsigned char* p = (unsigned char*)buf;
    size_t r = 0;
    while (r < len) {
        ssize_t n;
        if (c->ssl) n = SSL_read(c->ssl, p + r, (int)(len - r));
        else        n = recv(c->fd, p + r, len - r, 0);
        if (n <= 0) return -1;
        r += (size_t)n;
    }
    return 0;
}

static uint32_t to_be32_s(uint32_t v)   { return htonl(v); }
static uint32_t from_be32_s(uint32_t v) { return ntohl(v); }

static int send_header(Conn* c, uint8_t type, uint32_t payload_len, const char* image_id) {
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

static int recv_header(Conn* c, MessageHeader* out) {
    if (cs_recv_all(c, out, sizeof(*out)) != 0) return -1;
    out->length = from_be32_s(out->length);
    out->image_id[36] = '\0';
    return 0;
}

static int send_message(Conn* c, uint8_t type, const char* image_id,
                        const void* payload, uint32_t payload_len) {
    if (send_header(c, type, payload_len, image_id) != 0) return -1;
    if (payload_len > 0 && payload) {
        if (cs_send_all(c, payload, payload_len) != 0) return -1;
    }
    return 0;
}

// --- TLS init/cleanup ---
static int tls_init_ctx(void) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    g_ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!g_ssl_ctx) return -1;

    // Rutas por defecto (no ampliamos config; solo lo justo para habilitar TLS)
    const char* crt = "assets/tls/server.crt";
    const char* key = "assets/tls/server.key";

    if (SSL_CTX_use_certificate_file(g_ssl_ctx, crt, SSL_FILETYPE_PEM) != 1) return -1;
    if (SSL_CTX_use_PrivateKey_file(g_ssl_ctx, key, SSL_FILETYPE_PEM) != 1) return -1;
    if (!SSL_CTX_check_private_key(g_ssl_ctx)) return -1;

    return 0;
}

static void conn_close(Conn* c) {
    if (!c) return;
    if (c->ssl) { SSL_shutdown(c->ssl); SSL_free(c->ssl); c->ssl = NULL; }
    if (c->fd >= 0) { close(c->fd); c->fd = -1; }
}

// --- Rutina por conexión (un hilo por cliente) ---
static void* handle_client(void* arg) {
    Conn* c = (Conn*)arg;

    // Estado por imagen
    char     current_uuid[37] = {0};
    char     current_filename[MAX_FILENAME] = {0};
    char     current_format[10] = {0};
    FILE*    out = NULL;
    uint32_t expected_chunks = 0, received_chunks = 0;
    uint32_t remaining_bytes = 0;

    int done = 0;
    while (!done) {
        MessageHeader h;
        if (recv_header(c, &h) != 0) {
            log_line("Connection dropped while waiting header");
            break;
        }

        if (h.type == MSG_HELLO) {
            // Generar UUID y responder
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
            if (cs_recv_all(c, &info, sizeof(info)) != 0) {
                log_line("Failed to read IMAGE_INFO payload");
                break;
            }

            uint32_t total_size = from_be32_s(info.total_size);
            expected_chunks = from_be32_s(info.total_chunks);
            strncpy(current_filename, info.filename, sizeof(current_filename)-1);
            strncpy(current_format,  info.format,  sizeof(current_format)-1);
            current_filename[sizeof(current_filename)-1] = '\0';
            current_format[sizeof(current_format)-1]     = '\0';

            char outpath[512];
            snprintf(outpath, sizeof(outpath), "assets/incoming/%s_%s", h.image_id, current_filename);
            out = fopen(outpath, "wb");
            if (!out) {
                log_line("Failed to open output file %s: %s", outpath, strerror(errno));
                break;
            }
            remaining_bytes = total_size;

            log_line("IMAGE_INFO: id=%s file=%s size=%u bytes chunks=%u proc=%u fmt=%s",
                h.image_id, current_filename, total_size, expected_chunks,
                (unsigned)info.processing_type, current_format);

        } else if (h.type == MSG_IMAGE_CHUNK) {
            if (!out) { log_line("CHUNK without open file"); break; }
            size_t to_read = h.length;
            unsigned char* buf = (unsigned char*)malloc(to_read);
            if (!buf) { log_line("OOM on chunk"); break; }
            if (cs_recv_all(c, buf, to_read) != 0) {
                free(buf);
                log_line("Failed to read chunk body");
                break;
            }
            size_t w = fwrite(buf, 1, to_read, out);
            free(buf);
            if (w != to_read) {
                log_line("Write error");
                break;
            }
            received_chunks++;
            if (remaining_bytes >= to_read) remaining_bytes -= (uint32_t)to_read;
            else remaining_bytes = 0;

        } else if (h.type == MSG_IMAGE_COMPLETE) {
            // Consumir posible payload (formato como string)
            char fmt[32] = {0};
            if (h.length > 0 && h.length < sizeof(fmt)) {
                if (cs_recv_all(c, fmt, h.length) != 0) {
                    log_line("Failed read COMPLETE fmt");
                    break;
                }
                fmt[sizeof(fmt)-1] = '\0';
            } else if (h.length > 0) {
                // Si vino más de lo esperado, solo consumirlo
                char* tmp = (char*)malloc(h.length);
                if (!tmp) break;
                if (cs_recv_all(c, tmp, h.length) != 0) { free(tmp); break; }
                free(tmp);
            }

            if (out) { fclose(out); out = NULL; }

            log_line("IMAGE_COMPLETE: id=%s file=%s fmt=%s chunks=%u remaining=%u",
                     h.image_id, current_filename, fmt[0]?fmt:current_format,
                     received_chunks, remaining_bytes);

            // === ACK FINAL ===
            if (send_message(c, MSG_ACK, h.image_id, NULL, 0) != 0) {
                log_line("Failed sending final ACK");
                break;
            }

            // Reset de estado para aceptar otra imagen en la MISMA conexión si el cliente quiere
            current_uuid[0] = 0;
            current_filename[0] = 0;
            current_format[0] = 0;
            expected_chunks = received_chunks = remaining_bytes = 0;

        } else {
            // Desconocido: consumir payload si existiera
            if (h.length > 0) {
                char* tmp = (char*)malloc(h.length);
                if (!tmp) break;
                if (cs_recv_all(c, tmp, h.length) != 0) { free(tmp); break; }
                free(tmp);
            }
            log_line("Unknown msg type %u, ignored", (unsigned)h.type);
        }
    }

    if (out) { fclose(out); out = NULL; }
    conn_close(c);
    free(c);
    log_line("Connection closed");
    return NULL;
}

int main(int argc, char** argv) {
    ensure_dirs();
    g_log = fopen("assets/log.txt", "a");
    if (!g_log) {
        perror("open log");
        return 1;
    }

    // Activar TLS si se solicita con --tls o SERVER_TLS=1
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--tls") == 0) g_tls_enabled = 1;
    }
    const char* env_tls = getenv("SERVER_TLS");
    if (env_tls && strcmp(env_tls, "1") == 0) g_tls_enabled = 1;

    if (g_tls_enabled) {
        if (tls_init_ctx() != 0) {
            log_line("TLS requested but initialization failed. Check assets/tls/server.crt & server.key");
            return 1;
        }
        log_line("TLS enabled (listening TLS) on port %d", LISTEN_PORT);
    } else {
        log_line("Server starting (plain TCP) on port %d", LISTEN_PORT);
    }

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(LISTEN_PORT);

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
        perror("bind");
        close(srv);
        return 1;
    }
    if (listen(srv, BACKLOG) != 0) {
        perror("listen");
        close(srv);
        return 1;
    }
    log_line("Listening...");

    for (;;) {
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        int fd = accept(srv, (struct sockaddr*)&cli, &clilen);
        if (fd < 0) { perror("accept"); continue; }

        char cip[64];
        inet_ntop(AF_INET, &cli.sin_addr, cip, sizeof(cip));
        log_line("Accepted connection from %s:%d", cip, ntohs(cli.sin_port));

        // Preparar conexión (con o sin TLS) y lanzar hilo
        Conn* c = (Conn*)calloc(1, sizeof(Conn));
        if (!c) { close(fd); continue; }
        c->fd = fd;
        c->ssl = NULL;

        if (g_tls_enabled) {
            SSL* ssl = SSL_new(g_ssl_ctx);
            if (!ssl) { close(fd); free(c); continue; }
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

    // Nunca llegamos aquí normalmente
    fclose(g_log);
    if (g_ssl_ctx) { SSL_CTX_free(g_ssl_ctx); g_ssl_ctx = NULL; }
    return 0;
}