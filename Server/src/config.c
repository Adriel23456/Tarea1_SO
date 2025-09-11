#include "config.h"
#include "utils.h"
#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>

void set_default_config(ServerConfig* c) {
    c->port = DEFAULT_PORT;
    c->tls_enabled = 0;
    strncpy(c->tls_dir,      "assets/tls",        sizeof(c->tls_dir));
    strncpy(c->log_file,     "assets/log.txt",    sizeof(c->log_file));
    strncpy(c->histogram_dir,"assets/histogram",  sizeof(c->histogram_dir));
    strncpy(c->colors_red,   "assets/colors/red", sizeof(c->colors_red));
    strncpy(c->colors_green, "assets/colors/green", sizeof(c->colors_green));
    strncpy(c->colors_blue,  "assets/colors/blue", sizeof(c->colors_blue));

    c->tls_dir[sizeof(c->tls_dir)-1] = '\0';
    c->log_file[sizeof(c->log_file)-1] = '\0';
    c->histogram_dir[sizeof(c->histogram_dir)-1] = '\0';
    c->colors_red[sizeof(c->colors_red)-1] = '\0';
    c->colors_green[sizeof(c->colors_green)-1] = '\0';
    c->colors_blue[sizeof(c->colors_blue)-1] = '\0';
}

int load_config_json(const char* path, ServerConfig* c) {
    set_default_config(c);

    FILE* f = fopen(path, "rb");
    if (!f) {
        return -1; // File not found; caller may create default
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

    // Parse server section
    struct json_object *js_server = NULL, *js_paths = NULL;
    if (json_object_object_get_ex(root, "server", &js_server)) {
        struct json_object *jport = NULL, *jtls = NULL, *jtlsdir = NULL;
        
        if (json_object_object_get_ex(js_server, "port", &jport))
            c->port = json_object_get_int(jport);

        if (json_object_object_get_ex(js_server, "tls_enabled", &jtls))
            c->tls_enabled = json_object_get_int(jtls);

        if (json_object_object_get_ex(js_server, "tls_dir", &jtlsdir)) {
            const char* s = json_object_get_string(jtlsdir);
            if (s) { 
                strncpy(c->tls_dir, s, sizeof(c->tls_dir)-1); 
                c->tls_dir[sizeof(c->tls_dir)-1] = '\0'; 
            }
        }
    }

    // Parse paths section
    if (json_object_object_get_ex(root, "paths", &js_paths)) {
        struct json_object *jlog = NULL, *jhist = NULL, *jcolors = NULL;

        if (json_object_object_get_ex(js_paths, "log_file", &jlog)) {
            const char* s = json_object_get_string(jlog);
            if (s) {
                strncpy(c->log_file, s, sizeof(c->log_file)-1);
                c->log_file[sizeof(c->log_file)-1] = '\0';
            }
        }

        if (json_object_object_get_ex(js_paths, "histogram_dir", &jhist)) {
            const char* s = json_object_get_string(jhist);
            if (s) {
                strncpy(c->histogram_dir, s, sizeof(c->histogram_dir)-1);
                c->histogram_dir[sizeof(c->histogram_dir)-1] = '\0';
            }
        }

        if (json_object_object_get_ex(js_paths, "colors_dir", &jcolors)) {
            struct json_object *jr=NULL, *jg=NULL, *jb=NULL;

            if (json_object_object_get_ex(jcolors, "red", &jr)) {
                const char* s = json_object_get_string(jr);
                if (s) {
                    strncpy(c->colors_red, s, sizeof(c->colors_red)-1);
                    c->colors_red[sizeof(c->colors_red)-1] = '\0';
                }
            }

            if (json_object_object_get_ex(jcolors, "green", &jg)) {
                const char* s = json_object_get_string(jg);
                if (s) {
                    strncpy(c->colors_green, s, sizeof(c->colors_green)-1);
                    c->colors_green[sizeof(c->colors_green)-1] = '\0';
                }
            }

            if (json_object_object_get_ex(jcolors, "blue", &jb)) {
                const char* s = json_object_get_string(jb);
                if (s) {
                    strncpy(c->colors_blue, s, sizeof(c->colors_blue)-1);
                    c->colors_blue[sizeof(c->colors_blue)-1] = '\0';
                }
            }
        }
    }

    json_object_put(root);
    return 0;
}

int ensure_dirs_from_config(const ServerConfig* c) {
    if (ensure_parent_dir(c->log_file) != 0) return -1;
    if (mkdir_p(c->histogram_dir, 0755) != 0) return -1;
    if (mkdir_p(c->colors_red, 0755) != 0) return -1;
    if (mkdir_p(c->colors_green, 0755) != 0) return -1;
    if (mkdir_p(c->colors_blue, 0755) != 0) return -1;
    if (mkdir_p(c->tls_dir, 0755) != 0) return -1;
    return 0;
}