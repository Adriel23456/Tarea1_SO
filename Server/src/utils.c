#include "utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <libgen.h>
#include <arpa/inet.h>

/*
 * to_be32_s / from_be32_s
 * -----------------------
 * Helpers to convert 32-bit integers to/from network byte order.
 */
uint32_t to_be32_s(uint32_t v) { 
    return htonl(v); 
}

uint32_t from_be32_s(uint32_t v) { 
    return ntohl(v); 
}

/*
 * mkdir_p
 * -------
 * Create directories recursively (like `mkdir -p`). Returns 0 on
 * success, -1 on failure. `mode` is applied to newly created dirs.
 */
int mkdir_p(const char* path, mode_t mode) {
    // Create all components of 'path' recursively
    if (!path || !*path) return -1;

    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp)-1);
    tmp[sizeof(tmp)-1] = '\0';

    // Remove trailing slash (except for root "/")
    size_t len = strlen(tmp);
    if (len > 1 && tmp[len-1] == '/') 
        tmp[len-1] = '\0';

    // Create each directory component
    for (char* p = tmp + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
            if (mkdir(tmp, mode) != 0 && errno != EEXIST) 
                return -1;
            *p = '/';
        }
    }
    
    // Create final directory
    if (mkdir(tmp, mode) != 0 && errno != EEXIST) 
        return -1;
        
    return 0;
}

/*
 * ensure_parent_dir
 * -----------------
 * Ensure the parent directory of `file_path` exists. Returns 0 on
 * success or -1 on failure.
 */
int ensure_parent_dir(const char* file_path) {
    // Create parent directory for a file path
    char buf[1024];
    strncpy(buf, file_path, sizeof(buf)-1);
    buf[sizeof(buf)-1] = '\0';
    
    char* d = dirname(buf);
    return mkdir_p(d, 0755);
}

/*
 * read_file_fully
 * ----------------
 * Read an entire file into a newly allocated buffer. On success
 * returns the buffer and sets *out_len to its size. Caller must free
 * the returned buffer. Returns NULL on error.
 */
unsigned char* read_file_fully(const char* path, int* out_len) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz < 0) { 
        fclose(f); 
        return NULL; 
    }
    fseek(f, 0, SEEK_SET);
    
    unsigned char* buf = (unsigned char*)malloc((size_t)sz);
    if (!buf) { 
        fclose(f); 
        return NULL; 
    }
    
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); 
        fclose(f); 
        return NULL;
    }
    
    fclose(f);
    *out_len = (int)sz;
    return buf;
}