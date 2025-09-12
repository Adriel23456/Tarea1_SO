// server.h
#ifndef SERVER_H
#define SERVER_H

#include "connection.h"
#include <signal.h>

extern volatile sig_atomic_t g_terminate;
extern volatile sig_atomic_t g_reload;

void* handle_client(void* arg);
int start_server(void);
void server_request_shutdown(void);

#endif // SERVER_H