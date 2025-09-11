// Servidor TCP secuencial que recibe imágenes por nuestro protocolo.
// Guarda cada imagen en assets/incoming/<uuid>_<filename>
// y escribe eventos en assets/log.txt
//
// Compilar con libuuid: -luuid

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
#include "protocol.h"

#define LISTEN_PORT DEFAULT_PORT
#define BACKLOG 10

static FILE* g_log = NULL;

static void ensure_dirs(void) {
    mkdir("assets", 0755);
    mkdir("assets/incoming", 0755);
}

static void log_line(const char* fmt, ...) {
    if (!g_log) return;
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
}

static int send_all(int fd, const void* buf, size_t len) {
    const unsigned char* p = (const unsigned char*)buf;
    size_t s = 0;
    while (s < len) {
        ssize_t n = send(fd, p + s, len - s, 0);
        if (n <= 0) return -1;
        s += (size_t)n;
    }
    return 0;
}
static int recv_all(int fd, void* buf, size_t len) {
    unsigned char* p = (unsigned char*)buf;
    size_t r = 0;
    while (r < len) {
        ssize_t n = recv(fd, p + r, len - r, 0);
        if (n <= 0) return -1;
        r += (size_t)n;
    }
    return 0;
}

static uint32_t to_be32_s(uint32_t v) { return htonl(v); }
static uint32_t from_be32_s(uint32_t v) { return ntohl(v); }

static int send_header(int fd, uint8_t type, uint32_t payload_len, const char* image_id) {
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
    return send_all(fd, &h, sizeof(h));
}
static int recv_header(int fd, MessageHeader* out) {
    if (recv_all(fd, out, sizeof(*out)) != 0) return -1;
    out->length = from_be32_s(out->length);
    out->image_id[36] = '\0';
    return 0;
}

static int send_message(int fd, uint8_t type, const char* image_id,
                        const void* payload, uint32_t payload_len) {
    if (send_header(fd, type, payload_len, image_id) != 0) return -1;
    if (payload_len > 0 && payload) {
        if (send_all(fd, payload, payload_len) != 0) return -1;
    }
    return 0;
}

int main(void) {
    ensure_dirs();
    g_log = fopen("assets/log.txt", "a");
    if (!g_log) {
        perror("open log");
        return 1;
    }
    log_line("Server starting on port %d", LISTEN_PORT);

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

        // Estado por imagen
        char current_uuid[37] = {0};
        char current_filename[MAX_FILENAME] = {0};
        FILE* out = NULL;
        uint32_t expected_chunks = 0, received_chunks = 0;
        uint32_t remaining_bytes = 0;
        char current_format[10] = {0};

        int done = 0;
        while (!done) {
            MessageHeader h;
            if (recv_header(fd, &h) != 0) {
                log_line("Connection dropped while waiting header");
                break;
            }

            if (h.type == MSG_HELLO) {
                // Generar UUID y responder
                uuid_t uu;
                uuid_generate(uu);
                uuid_unparse_lower(uu, current_uuid);
                log_line("HELLO -> new image id = %s", current_uuid);

                if (send_message(fd, MSG_IMAGE_ID_RESPONSE, current_uuid, NULL, 0) != 0) {
                    log_line("Failed sending IMAGE_ID_RESPONSE");
                    break;
                }
            } else if (h.type == MSG_IMAGE_INFO) {
                if (h.length != sizeof(ImageInfo)) {
                    log_line("IMAGE_INFO wrong size %u", h.length);
                    break;
                }
                ImageInfo info;
                if (recv_all(fd, &info, sizeof(info)) != 0) {
                    log_line("Failed to read IMAGE_INFO payload");
                    break;
                }
                // Decodificar campos BE
                uint32_t total_size = from_be32_s(info.total_size);
                expected_chunks = from_be32_s(info.total_chunks);
                strncpy(current_filename, info.filename, sizeof(current_filename)-1);
                strncpy(current_format, info.format, sizeof(current_format)-1);
                current_filename[sizeof(current_filename)-1] = '\0';
                current_format[sizeof(current_format)-1] = '\0';

                // Abrir archivo destino
                char outpath[512];
                snprintf(outpath, sizeof(outpath), "assets/incoming/%s_%s", h.image_id, current_filename);
                out = fopen(outpath, "wb");
                if (!out) {
                    log_line("Failed to open output file %s: %s", outpath, strerror(errno));
                    break;
                }
                remaining_bytes = total_size;

                log_line("IMAGE_INFO: id=%s file=%s size=%u bytes chunks=%u proc=%u fmt=%s",
                    h.image_id, current_filename, total_size, expected_chunks, (unsigned)info.processing_type, current_format);

            } else if (h.type == MSG_IMAGE_CHUNK) {
                if (!out) { log_line("CHUNK without open file"); break; }
                // leer payload y volcar
                size_t to_read = h.length;
                unsigned char* buf = (unsigned char*)malloc(to_read);
                if (!buf) { log_line("OOM on chunk"); break; }
                if (recv_all(fd, buf, to_read) != 0) {
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
                // leer formato (string) y cerrar
                char fmt[32] = {0};
                if (h.length > 0 && h.length < sizeof(fmt)) {
                    if (recv_all(fd, fmt, h.length) != 0) { log_line("Failed read COMPLETE fmt"); break; }
                    fmt[sizeof(fmt)-1] = '\0';
                } else if (h.length > 0) {
                    // consumir sin guardar
                    char* tmp = (char*)malloc(h.length);
                    if (!tmp) break;
                    if (recv_all(fd, tmp, h.length) != 0) { free(tmp); break; }
                    free(tmp);
                }
                if (out) { fclose(out); out = NULL; }

                log_line("IMAGE_COMPLETE: id=%s file=%s fmt=%s chunks=%u remaining=%u",
                         h.image_id, current_filename, fmt[0]?fmt:current_format, received_chunks, remaining_bytes);

                // Reset estado para la siguiente imagen (si el cliente envía otra en misma conexión)
                current_uuid[0] = 0;
                current_filename[0] = 0;
                current_format[0] = 0;
                expected_chunks = received_chunks = remaining_bytes = 0;

                // No cortamos la conexión; permitimos múltiples imágenes si el cliente lo desea.
                // done = 1; // si quieres cerrar tras recibir una imagen, descomenta.
            } else {
                // Consumir payload si hubiera, luego ignorar
                if (h.length > 0) {
                    char* tmp = (char*)malloc(h.length);
                    if (!tmp) break;
                    if (recv_all(fd, tmp, h.length) != 0) { free(tmp); break; }
                    free(tmp);
                }
                log_line("Unknown msg type %u, ignored", (unsigned)h.type);
            }
        }

        if (out) { fclose(out); out = NULL; }
        close(fd);
        log_line("Connection closed");
    }

    fclose(g_log);
    return 0;
}
