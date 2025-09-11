/**
 * main.c
 * Image Processing Server
 * Simple TCP server that receives images from clients
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <errno.h>
#include "protocol.h"
#include "image_handler.h"
#include "logger.h"

#define MAX_CLIENTS 10

// Global variables
static int server_running = 1;
static int server_socket = -1;

// Client session structure
typedef struct {
    int socket_fd;
    struct sockaddr_in address;
    char current_image_id[37];
    uint8_t* image_buffer;
    size_t buffer_size;
    size_t received_size;
    ImageInfo image_info;
    uint32_t chunks_received;
} ClientSession;

// Signal handler
void signal_handler(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        printf("\nShutting down server...\n");
        server_running = 0;
        if (server_socket >= 0) {
            close(server_socket);
        }
    }
}

// Generate unique ID
char* generate_image_id() {
    static char id[37];
    sprintf(id, "%08x-%04x-%04x-%04x-%012lx",
            (unsigned int)time(NULL),
            (unsigned int)rand() & 0xFFFF,
            (unsigned int)rand() & 0xFFFF,
            (unsigned int)rand() & 0xFFFF,
            (unsigned long)rand());
    return id;
}

// Handle messages from client
void handle_message(ClientSession* session, MessageHeader* header) {
    MessageHeader response;
    
    switch(header->type) {
        case MSG_HELLO:
            log_event("Client %s: HELLO received", inet_ntoa(session->address.sin_addr));
            response.type = MSG_ACK;
            response.length = 0;
            strcpy(response.image_id, "HELLO_ACK");
            send(session->socket_fd, &response, sizeof(response), 0);
            break;
            
        case MSG_IMAGE_ID_REQUEST:
            {
                char* new_id = generate_image_id();
                strcpy(session->current_image_id, new_id);
                log_event("Client %s: Generated ID %s", 
                         inet_ntoa(session->address.sin_addr), new_id);
                
                response.type = MSG_IMAGE_ID_RESPONSE;
                response.length = 0;
                strcpy(response.image_id, new_id);
                send(session->socket_fd, &response, sizeof(response), 0);
            }
            break;
            
        case MSG_IMAGE_INFO:
            {
                // Receive image info
                if (recv(session->socket_fd, &session->image_info, 
                        sizeof(ImageInfo), MSG_WAITALL) <= 0) {
                    log_event("Client %s: Failed to receive image info", 
                             inet_ntoa(session->address.sin_addr));
                    return;
                }
                
                // Prepare buffer
                session->buffer_size = session->image_info.total_size;
                session->image_buffer = malloc(session->buffer_size);
                session->received_size = 0;
                session->chunks_received = 0;
                strcpy(session->current_image_id, header->image_id);
                
                const char* proc_type = "unknown";
                if (session->image_info.processing_type == PROC_HISTOGRAM) {
                    proc_type = "histogram";
                } else if (session->image_info.processing_type == PROC_COLOR_CLASSIFICATION) {
                    proc_type = "color_classification";
                } else if (session->image_info.processing_type == PROC_BOTH) {
                    proc_type = "both";
                }
                
                log_event("Client %s: Starting transfer of %s (%.2f MB, %u chunks, type: %s)", 
                         inet_ntoa(session->address.sin_addr),
                         session->image_info.filename,
                         session->buffer_size / (1024.0 * 1024.0),
                         session->image_info.total_chunks,
                         proc_type);
                
                // Send ACK
                response.type = MSG_ACK;
                response.length = 0;
                strcpy(response.image_id, session->current_image_id);
                send(session->socket_fd, &response, sizeof(response), 0);
            }
            break;
            
        case MSG_IMAGE_CHUNK:
            {
                uint32_t chunk_num;
                
                // Receive chunk number
                if (recv(session->socket_fd, &chunk_num, sizeof(uint32_t), MSG_WAITALL) <= 0) {
                    log_event("Client %s: Failed to receive chunk number", 
                             inet_ntoa(session->address.sin_addr));
                    return;
                }
                
                // Calculate size and offset
                size_t chunk_size = header->length - sizeof(uint32_t);
                size_t offset = chunk_num * CHUNK_SIZE;
                
                // Receive chunk data
                if (recv(session->socket_fd, session->image_buffer + offset, 
                        chunk_size, MSG_WAITALL) <= 0) {
                    log_event("Client %s: Failed to receive chunk data", 
                             inet_ntoa(session->address.sin_addr));
                    return;
                }
                
                session->received_size += chunk_size;
                session->chunks_received++;
                
                printf("Chunk %u/%u received (%.1f%% complete)\r", 
                       session->chunks_received,
                       session->image_info.total_chunks,
                       (session->received_size * 100.0) / session->buffer_size);
                fflush(stdout);
                
                // Send ACK
                response.type = MSG_ACK;
                response.length = 0;
                strcpy(response.image_id, session->current_image_id);
                send(session->socket_fd, &response, sizeof(response), 0);
            }
            break;
            
        case MSG_IMAGE_COMPLETE:
            {
                printf("\n");
                log_event("Client %s: Image %s complete (%.2f MB received)", 
                         inet_ntoa(session->address.sin_addr),
                         session->image_info.filename,
                         session->received_size / (1024.0 * 1024.0));
                
                // Save image
                save_image(session->current_image_id, 
                          session->image_info.filename,
                          session->image_info.format,
                          session->image_info.processing_type,
                          session->image_buffer,
                          session->received_size);
                
                // Clean up
                free(session->image_buffer);
                session->image_buffer = NULL;
                session->buffer_size = 0;
                session->received_size = 0;
                
                // Send ACK
                response.type = MSG_ACK;
                response.length = 0;
                strcpy(response.image_id, session->current_image_id);
                send(session->socket_fd, &response, sizeof(response), 0);
            }
            break;
            
        default:
            log_event("Client %s: Unknown message type %d", 
                     inet_ntoa(session->address.sin_addr), header->type);
            break;
    }
}

// Client handler thread
void* client_handler(void* arg) {
    ClientSession* session = (ClientSession*)arg;
    
    log_event("Client connected from %s:%d", 
             inet_ntoa(session->address.sin_addr),
             ntohs(session->address.sin_port));
    
    while (server_running) {
        MessageHeader header;
        ssize_t bytes = recv(session->socket_fd, &header, sizeof(header), MSG_WAITALL);
        
        if (bytes <= 0) {
            if (bytes == 0) {
                log_event("Client %s disconnected", inet_ntoa(session->address.sin_addr));
            } else {
                log_event("Client %s: recv error: %s", 
                         inet_ntoa(session->address.sin_addr), strerror(errno));
            }
            break;
        }
        
        handle_message(session, &header);
    }
    
    // Clean up
    if (session->image_buffer) {
        free(session->image_buffer);
    }
    close(session->socket_fd);
    free(session);
    return NULL;
}

int main(int argc, char* argv[]) {
    int port = DEFAULT_PORT;
    
    // Parse arguments
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0) {
            port = DEFAULT_PORT;
        }
    }
    
    // Initialize
    init_logger();
    init_directories();
    
    // Set up signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    printf("========================================\n");
    printf("     IMAGE PROCESSING SERVER\n");
    printf("========================================\n");
    printf("Port: %d\n", port);
    printf("Press Ctrl+C to stop\n");
    printf("========================================\n\n");
    
    log_event("Server starting on port %d", port);
    
    // Create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Socket creation failed");
        return 1;
    }
    
    // Allow reuse
    int opt = 1;
    setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // Bind
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_socket);
        return 1;
    }
    
    // Listen
    if (listen(server_socket, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_socket);
        return 1;
    }
    
    printf("Server listening on port %d...\n\n", port);
    log_event("Server ready and listening");
    
    // Accept connections
    while (server_running) {
        ClientSession* session = malloc(sizeof(ClientSession));
        memset(session, 0, sizeof(ClientSession));
        
        socklen_t addr_len = sizeof(session->address);
        session->socket_fd = accept(server_socket, 
                                   (struct sockaddr*)&session->address, 
                                   &addr_len);
        
        if (session->socket_fd < 0) {
            if (server_running) {
                perror("Accept failed");
            }
            free(session);
            continue;
        }
        
        // Create thread for client
        pthread_t thread;
        if (pthread_create(&thread, NULL, client_handler, session) != 0) {
            perror("Thread creation failed");
            close(session->socket_fd);
            free(session);
            continue;
        }
        
        pthread_detach(thread);
    }
    
    // Cleanup
    if (server_socket >= 0) {
        close(server_socket);
    }
    
    log_event("Server stopped");
    close_logger();
    
    printf("\nServer stopped.\n");
    return 0;
}