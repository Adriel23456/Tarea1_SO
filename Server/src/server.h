#ifndef SERVER_H
#define SERVER_H

#include "connection.h"

// Handle a single client connection
void* handle_client(void* arg);

// Start the server and listen for connections
int start_server(void);

#endif // SERVER_H