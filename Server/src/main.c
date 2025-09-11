// Concurrent TCP/TLS image server with final ACK, TLS (OpenSSL), and thread-per-connection.
// Saves to assets/incoming/<uuid>_<filename> and logs to the configured log file.
//
// New: reads server configuration from assets/config.json (all in English):
// {
//   "server": {
//     "port": 1717,
//     "tls_enabled": 1,
//     "tls_dir": "assets/tls"
//   },
//   "paths": {
//     "log_file": "assets/log.txt",
//     "incoming_dir": "assets/incoming",
//     "histogram_dir": "assets/histogram",
//     "colors_dir": {
//       "red": "assets/colors/red",
//       "green": "assets/colors/green",
//       "blue": "assets/colors/blue"
//     }
//   }
// }
//
// Build (Makefile updated):
//   LIBS = -luuid -lssl -lcrypto -lpthread -ljson-c

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
#include <libgen.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <json-c/json.h>
#include "protocol.h"

#define BACKLOG 10

// -------- Config structure --------
typedef struct {
    int   port;
    int   tls_enabled;     // 1 or 0
    char  tls_dir[512];    // e.g., assets/tls
    char  log_file[512];   // e.g., assets/log.txt
    char  incoming_dir[512];
    char  histogram_dir[512];
    char  colors_red[512];
    char  colors_green[512];
    char  colors_blue[512];
} ServerConfig;

static ServerConfig g_cfg;

// -------- Logging --------
static FILE* g_log = NULL;
static pthread_mutex_t g_log_mtx = PTHREAD_MUTEX_INITIALIZER;

// -------- TLS globals --------
static SSL_CTX* g_ssl_ctx = NULL;

// -------- Utilities --------
static uint32_t to_be32_s(uint32_t v)   { return htonl(v); }
static uint32_t from_be32_s(uint32_t v) { return ntohl(v); }

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

static int mkdir_p(const char* path, mode_t mode) {
    // Create all components of 'path' (like `mkdir -p`).
    if (!path || !*path) return -1;

    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';

    // If ends with '/', remove it (except root "/")
    size_t len = strlen(tmp);
    if (len > 1 && tmp[len-1] == '/') tmp[len-1] = '\0';

    for (char* p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
            *p = '/';
        }
    }
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) return -1;
    return 0;
}

static int ensure_parent_dir(const char* file_path) {
    // Create parent directory for a file path
    char buf[1024];
    strncpy(buf, file_path, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
    char* d = dirname(buf);
    return mkdir_p(d, 0755);
}

// -------- JSON config --------
static void set_default_config(ServerConfig* c) {
    c->port = DEFAULT_PORT;
    c->tls_enabled = 0;
    strncpy(c->tls_dir,      "assets/tls",        sizeof(c->tls_dir));
    strncpy(c->log_file,     "assets/log.txt",    sizeof(c->log_file));
    strncpy(c->incoming_dir, "assets/incoming",   sizeof(c->incoming_dir));
    strncpy(c->histogram_dir,"assets/histogram",  sizeof(c->histogram_dir));
    strncpy(c->colors_red,   "assets/colors/red", sizeof(c->colors_red));
    strncpy(c->colors_green, "assets/colors/green", sizeof(c->colors_green));
    strncpy(c->colors_blue,  "assets/colors/blue", sizeof(c->colors_blue));
    // null-terminate
    c->tls_dir[sizeof(c->tls_dir)-1] = '\0';
    c->log_file[sizeof(c->log_file)-1] = '\0';
    c->incoming_dir[sizeof(c->incoming_dir)-1] = '\0';
    c->histogram_dir[sizeof(c->histogram_dir)-1] = '\0';
    c->colors_red[sizeof(c->colors_red)-1] = '\0';
    c->colors_green[sizeof(c->colors_green)-1] = '\0';
    c->colors_blue[sizeof(c->colors_blue)-1] = '\0';
}

static int load_config_json(const char* path, ServerConfig* c) {
    set_default_config(c);

    FILE* f = fopen(path, "rb");
    if (!f) {
        return -1; // not found; caller may create default file
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return -1; }
    fseek(f, 0, SEEK_SET);

    char* buf = (char*)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return -1;
    }
    buf[sz] = '\0';
    fclose(f);

    struct json_object* root = json_tokener_parse(buf);
    free(buf);
    if (!root) return -1;

    struct json_object *js_server = NULL, *js_paths = NULL;
    if (json_object_object_get_ex(root, "server", &js_server)) {
        struct json_object *jport = NULL, *jtls = NULL, *jtlsdir = NULL;
        if (json_object_object_get_ex(js_server, "port", &jport))
            c->port = json_object_get_int(jport);

        if (json_object_object_get_ex(js_server, "tls_enabled", &jtls))
            c->tls_enabled = json_object_get_int(jtls);

        if (json_object_object_get_ex(js_server, "tls_dir", &jtlsdir)) {
            const char* s = json_object_get_string(jtlsdir);
            if (s) { strncpy(c->tls_dir, s, sizeof(c->tls_dir)-1); c->tls_dir[sizeof(c->tls_dir)-1] = '\0'; }
        }
    }

    if (json_object_object_get_ex(root, "paths", &js_paths)) {
        struct json_object *jlog = NULL, *jincoming = NULL, *jhist = NULL, *jcolors = NULL;
        if (json_object_object_get_ex(js_paths, "log_file", &jlog)) {
            const char* s = json_object_get_string(jlog);
            if (s) { strncpy(c->log_file, s, sizeof(c->log_file)-1); c->log_file[sizeof(c->log_file)-1] = '\0'; }
        }
        if (json_object_object_get_ex(js_paths, "incoming_dir", &jincoming)) {
            const char* s = json_object_get_string(jincoming);
            if (s) { strncpy(c->incoming_dir, s, sizeof(c->incoming_dir)-1); c->incoming_dir[sizeof(c->incoming_dir)-1] = '\0'; }
        }
        if (json_object_object_get_ex(js_paths, "histogram_dir", &jhist)) {
            const char* s = json_object_get_string(jhist);
            if (s) { strncpy(c->histogram_dir, s, sizeof(c->histogram_dir)-1); c->histogram_dir[sizeof(c->histogram_dir)-1] = '\0'; }
        }
        if (json_object_object_get_ex(js_paths, "colors_dir", &jcolors)) {
            struct json_object *jr=NULL, *jg=NULL, *jb=NULL;
            if (json_object_object_get_ex(jcolors, "red", &jr)) {
                const char* s = json_object_get_string(jr);
                if (s) { strncpy(c->colors_red, s, sizeof(c->colors_red)-1); c->colors_red[sizeof(c->colors_red)-1] = '\0'; }
            }
            if (json_object_object_get_ex(jcolors, "green", &jg)) {
                const char* s = json_object_get_string(jg);
                if (s) { strncpy(c->colors_green, s, sizeof(c->colors_green)-1); c->colors_green[sizeof(c->colors_green)-1] = '\0'; }
            }
            if (json_object_object_get_ex(jcolors, "blue", &jb)) {
                const char* s = json_object_get_string(jb);
                if (s) { strncpy(c->colors_blue, s, sizeof(c->colors_blue)-1); c->colors_blue[sizeof(c->colors_blue)-1] = '\0'; }
            }
        }
    }

    json_object_put(root);
    return 0;
}

// Create required directories based on config.
// Note: log_file is a file path; others are directories.
static int ensure_dirs_from_config(const ServerConfig* c) {
    if (ensure_parent_dir(c->log_file) != 0) return -1;
    if (mkdir_p(c->incoming_dir, 0755) != 0) return -1;
    if (mkdir_p(c->histogram_dir, 0755) != 0) return -1;
    if (mkdir_p(c->colors_red, 0755) != 0) return -1;
    if (mkdir_p(c->colors_green, 0755) != 0) return -1;
    if (mkdir_p(c->colors_blue, 0755) != 0) return -1;
    if (mkdir_p(c->tls_dir, 0755) != 0) return -1;
    return 0;
}

// -------- Connection abstraction (plain/TLS) --------
typedef struct {
    int  fd;
    SSL* ssl; // NULL if plain
} Conn;

static int cs_send_all(Conn* c, const void* buf, size_t len) {
    const unsigned char* p = (const unsigned char*)buf;
    size_t s = 0;
    while (s < len) {
        ssize_t n = c->ssl ? SSL_write(c->ssl, p + s, (int)(len - s))
                           : send(c->fd, p + s, len - s, 0);
        if (n <= 0) return -1;
        s += (size_t)n;
    }
    return 0;
}

static int cs_recv_all(Conn* c, void* buf, size_t len) {
    unsigned char* p = (unsigned char*)buf;
    size_t r = 0;
    while (r < len) {
        ssize_t n = c->ssl ? SSL_read(c->ssl, p + r, (int)(len - r))
                           : recv(c->fd, p + r, len - r, 0);
        if (n <= 0) return -1;
        r += (size_t)n;
    }
    return 0;
}

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

static void conn_close(Conn* c) {
    if (!c) return;
    if (c->ssl) { SSL_shutdown(c->ssl); SSL_free(c->ssl); c->ssl = NULL; }
    if (c->fd >= 0) { close(c->fd); c->fd = -1; }
}

// -------- TLS init using config --------
static int tls_init_ctx(void) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();

    g_ssl_ctx = SSL_CTX_new(TLS_server_method());
    if (!g_ssl_ctx) return -1;

    char crt[1024], key[1024];
    snprintf(crt, sizeof(crt), "%s/server.crt", g_cfg.tls_dir);
    snprintf(key, sizeof(key), "%s/server.key", g_cfg.tls_dir);

    if (SSL_CTX_use_certificate_file(g_ssl_ctx, crt, SSL_FILETYPE_PEM) != 1) return -1;
    if (SSL_CTX_use_PrivateKey_file(g_ssl_ctx, key, SSL_FILETYPE_PEM) != 1) return -1;
    if (!SSL_CTX_check_private_key(g_ssl_ctx)) return -1;

    return 0;
}

// -------- Per-connection thread --------
static void* handle_client(void* arg) {
    Conn* c = (Conn*)arg;

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

            // Open output file under configured incoming_dir
            char outpath[1024];
            snprintf(outpath, sizeof(outpath), "%s/%s_%s", g_cfg.incoming_dir, h.image_id, current_filename);
            if (ensure_parent_dir(outpath) != 0) {
                log_line("Failed to create parent dir for %s", outpath);
                break;
            }
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
            char fmt[32] = {0};
            if (h.length > 0 && h.length < sizeof(fmt)) {
                if (cs_recv_all(c, fmt, h.length) != 0) {
                    log_line("Failed read COMPLETE fmt");
                    break;
                }
                fmt[sizeof(fmt)-1] = '\0';
            } else if (h.length > 0) {
                char* tmp = (char*)malloc(h.length);
                if (!tmp) break;
                if (cs_recv_all(c, tmp, h.length) != 0) { free(tmp); break; }
                free(tmp);
            }

            if (out) { fclose(out); out = NULL; }

            log_line("IMAGE_COMPLETE: id=%s file=%s fmt=%s chunks=%u remaining=%u",
                     h.image_id, current_filename, fmt[0]?fmt:current_format,
                     received_chunks, remaining_bytes);

            // Final ACK
            if (send_message(c, MSG_ACK, h.image_id, NULL, 0) != 0) {
                log_line("Failed sending final ACK");
                break;
            }

            // Reset to allow another image on same connection
            current_uuid[0]   = 0;
            current_filename[0] = 0;
            current_format[0] = 0;
            expected_chunks = received_chunks = remaining_bytes = 0;

        } else {
            // Consume unknown payload if any, then ignore
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

int main(void) {
    // Load config
    if (load_config_json("assets/config.json", &g_cfg) != 0) {
        // If missing, create default directories and default log file,
        // but still try to start with defaults.
        set_default_config(&g_cfg);
    }

    // Ensure directories exist
    if (ensure_dirs_from_config(&g_cfg) != 0) {
        fprintf(stderr, "Failed to create required directories from config\n");
        return 1;
    }

    // Open log file
    g_log = fopen(g_cfg.log_file, "a");
    if (!g_log) {
        perror("open log_file");
        return 1;
    }

    // TLS init if enabled
    if (g_cfg.tls_enabled) {
        if (tls_init_ctx() != 0) {
            log_line("TLS enabled in config, but initialization failed. Check certificate and key in %s", g_cfg.tls_dir);
            return 1;
        }
        log_line("TLS enabled (listening TLS) on port %d", g_cfg.port);
    } else {
        log_line("Server starting (plain TCP) on port %d", g_cfg.port);
    }

    // Listen socket
    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(g_cfg.port);

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

    // Accept loop with thread-per-connection
    for (;;) {
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        int fd = accept(srv, (struct sockaddr*)&cli, &clilen);
        if (fd < 0) { perror("accept"); continue; }

        char cip[64];
        inet_ntop(AF_INET, &cli.sin_addr, cip, sizeof(cip));
        log_line("Accepted connection from %s:%d", cip, ntohs(cli.sin_port));

        Conn* c = (Conn*)calloc(1, sizeof(Conn));
        if (!c) { close(fd); continue; }
        c->fd = fd;
        c->ssl = NULL;

        if (g_cfg.tls_enabled) {
            SSL* ssl = SSL_new(g_ssl_ctx);
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

    // (Unreachable in normal run)
    if (g_ssl_ctx) { SSL_CTX_free(g_ssl_ctx); g_ssl_ctx = NULL; }
    fclose(g_log);
    return 0;
}