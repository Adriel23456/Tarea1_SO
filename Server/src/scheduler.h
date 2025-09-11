#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>
#include <stddef.h>
#include "protocol.h"

// Trabajo de procesamiento en memoria (smallest-first por total_size)
typedef struct {
    unsigned char* data;           // buffer con la imagen completa (propiedad del scheduler)
    size_t         size;           // bytes del buffer
    char           image_id[37];   // UUID
    char           filename[MAX_FILENAME];
    char           format[10];     // "jpg","jpeg","png","gif"
    ProcessingType processing_type;
    uint32_t       total_size;     // para prioridad (redundante con size pero explícito)
} ProcJob;

int scheduler_init(void);
int scheduler_enqueue(const ProcJob* job); // hace copia del descriptor; "data" debe ser asignado por el caller y pasa a ser propiedad del scheduler
void scheduler_shutdown(void);

#endif // SCHEDULER_H