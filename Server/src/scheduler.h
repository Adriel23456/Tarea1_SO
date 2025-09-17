#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h"

// In-memory processing job (smallest-first by total_size)
typedef struct {
    unsigned char* data;           // buffer containing the complete image (owned by the scheduler)
    size_t         size;           // bytes del buffer
    char           image_id[37];   // UUID
    char           filename[MAX_FILENAME];
    char           format[10];     // "jpg","jpeg","png","gif"
    ProcessingType processing_type;
    uint32_t       total_size;     // for priority (redundant with size but explicit)
} ProcJob;

int scheduler_init(void);
int scheduler_enqueue(const ProcJob* job); // makes a shallow copy of the descriptor; `data` must be allocated by the caller and becomes owned by the scheduler
void scheduler_shutdown(void);

#endif // SCHEDULER_H