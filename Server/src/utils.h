#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <sys/types.h>

// Network byte order conversion utilities
uint32_t to_be32_s(uint32_t v);
uint32_t from_be32_s(uint32_t v);

// Create directory path recursively (like mkdir -p)
// Returns: 0 on success, -1 on failure
int mkdir_p(const char* path, mode_t mode);

// Create parent directory for a file path
// Returns: 0 on success, -1 on failure
int ensure_parent_dir(const char* file_path);

// Read entire file into memory
// Returns: allocated buffer (must be freed by caller), NULL on failure
unsigned char* read_file_fully(const char* path, int* out_len);

#endif // UTILS_H