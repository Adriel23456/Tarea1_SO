/**
 * network.h
 * Network functionality header
 */

#ifndef NETWORK_H
#define NETWORK_H

#include <gtk/gtk.h>
#include "protocol.h"

// Progress callback
typedef void (*ProgressCallback)(const char* message, double progress);

// Functions
int send_all_images(GSList* image_list, const char* host, int port, 
                    ProcessingType proc_type, ProgressCallback callback);

#endif