#include <stdio.h>
#include <stdlib.h>
#include "config.h"
#include "logging.h"
#include "server.h"
#include "connection.h"
#include "scheduler.h"

// Global configuration instance
ServerConfig g_cfg;

// Provide external access to SSL context
extern SSL_CTX* get_ssl_ctx(void);

int main(void) {
    // Load configuration from JSON file
    if (load_config_json("assets/config.json", &g_cfg) != 0) {
        // If config file is missing, use defaults
        set_default_config(&g_cfg);
    }

    // Ensure all required directories exist
    if (ensure_dirs_from_config(&g_cfg) != 0) {
        fprintf(stderr, "Failed to create required directories from config\n");
        return 1;
    }

    // Initialize logging system
    if (log_init(g_cfg.log_file) != 0) {
        perror("Failed to initialize logging");
        return 1;
    }

    // ---- Iniciar scheduler (worker que escucha y procesa por tamaño) ----
    if (scheduler_init() != 0) {
        fprintf(stderr, "Failed to start scheduler worker\n");
        log_close();
        return 1;
    }

    // Start the server (acepta conexiones y encola trabajos)
    int result = start_server();

    // Cleanup
    scheduler_shutdown();   // <--- parar worker y liberar heap
    tls_cleanup();
    log_close();
    
    return result;
}