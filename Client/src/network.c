/**
 * network.c
 * Network functionality for the client (stub version)
 *
 * NOTA: Esta versión solo simula el envío por chunks leyendo del disco
 * y reportando progreso. No abre sockets. Sirve para compilar y probar
 * la GUI. Luego reemplazaremos el cuerpo por la implementación TCP/HTTP real.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <gtk/gtk.h>
#include "network.h"
#include "protocol.h"

static int send_one_image_stub(const char* filepath,
                               const char* host,
                               int port,
                               ProcessingType proc_type,
                               ProgressCallback cb) {
    (void)host;
    (void)port;
    (void)proc_type;

    FILE* f = fopen(filepath, "rb");
    if (!f) {
        if (cb) cb("Failed to open image file", 0.0);
        g_printerr("send_one_image_stub: cannot open %s: %s\n", filepath, strerror(errno));
        return -1;
    }

    // obtener tamaño
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        if (cb) cb("Failed to read image size", 0.0);
        return -1;
    }
    long total_size = ftell(f);
    if (total_size < 0) {
        fclose(f);
        if (cb) cb("Failed to read image size", 0.0);
        return -1;
    }
    rewind(f);

    // buffer de chunk
    unsigned char* buf = (unsigned char*)malloc(CHUNK_SIZE);
    if (!buf) {
        fclose(f);
        if (cb) cb("Out of memory", 0.0);
        return -1;
    }

    long sent = 0;
    if (cb) {
        char msg[256];
        g_snprintf(msg, sizeof(msg), "Preparing %s", filepath);
        cb(msg, 0.0);
    }

    // simular envío por chunks leyendo del archivo
    while (!feof(f)) {
        size_t n = fread(buf, 1, CHUNK_SIZE, f);
        if (ferror(f)) {
            free(buf);
            fclose(f);
            if (cb) cb("Read error", 0.0);
            return -1;
        }
        sent += (long)n;

        if (cb && total_size > 0) {
            double progress = (double)sent / (double)total_size;
            char msg[256];
            g_snprintf(msg, sizeof(msg), "Sending %s", filepath);
            cb(msg, progress > 1.0 ? 1.0 : progress);
        }
    }

    free(buf);
    fclose(f);

    if (cb) {
        char msg[256];
        g_snprintf(msg, sizeof(msg), "Finished %s", filepath);
        cb(msg, 1.0);
    }
    return 0;
}

int send_all_images(GSList* image_list,
                    const char* host,
                    int port,
                    ProcessingType proc_type,
                    ProgressCallback callback) {
    (void)host;  // se usan en el stub solo para logging; la versión real los usará.
    (void)port;

    int overall_rc = 0;
    for (GSList* it = image_list; it != NULL; it = it->next) {
        const char* path = (const char*)it->data;
        if (!path) continue;

        // por ahora simulamos
        int rc = send_one_image_stub(path, host, port, proc_type, callback);
        if (rc != 0) {
            overall_rc = rc; // conserva el primero que falla
            // si quieres detener ante el primer error, descomenta:
            // break;
        }
    }
    return overall_rc;
}
