/* main.c */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include "config.h"
#include "logging.h"
#include "server.h"
#include "connection.h"
#include "scheduler.h"
#include "daemon.h"   // NUEVO

// Global configuration
ServerConfig g_cfg;

// Señales: handlers
static const char* g_pidfile = NULL;

static void handle_sigterm(int sig) {
    (void)sig;
    server_request_shutdown();
}

static void handle_sighup(int sig) {
    (void)sig;
    g_reload = 1;
}

static void install_signal_handlers(void) {
    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sa.sa_handler = handle_sigterm;
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT,  &sa, NULL);

    sa.sa_handler = handle_sighup;
    sigaction(SIGHUP, &sa, NULL);
}

static void usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [--config <path>] [--daemon] [--pidfile <path>] [--foreground]\n"
        "       --config    Ruta a config.json (default: assets/config.json)\n"
        "       --daemon    Doble fork + PIDFile (modo clasico deamon)\n"
        "       --pidfile   Ruta del PIDFile (default: /run/ImageService.pid)\n"
        "       --foreground (por defecto) correr en foreground (ideal para systemd)\n",
        prog);
}

int main(int argc, char** argv) {
    const char* cfg_path = "assets/config.json";
    int use_daemon = 0;
    const char* pidfile = "/run/ImageService.pid";

    for (int i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "--config") && i+1 < argc) {
            cfg_path = argv[++i];
        } else if (!strcmp(argv[i], "--daemon")) {
            use_daemon = 1;
        } else if (!strcmp(argv[i], "--pidfile") && i+1 < argc) {
            pidfile = argv[++i];
        } else if (!strcmp(argv[i], "--foreground")) {
            use_daemon = 0;
        } else {
            usage(argv[0]);
            return 1;
        }
    }
    g_pidfile = use_daemon ? pidfile : NULL;

    // Carga config
    if (load_config_json(cfg_path, &g_cfg) != 0) set_default_config(&g_cfg);
    if (ensure_dirs_from_config(&g_cfg) != 0) {
        fprintf(stderr, "Failed to create required directories from config\n");
        return 1;
    }

    // Logging
    if (log_init(g_cfg.log_file) != 0) {
        perror("Failed to initialize logging");
        return 1;
    }

    // Señales
    install_signal_handlers();

    // Daemon clásico (opcional)
    if (use_daemon) {
        if (daemonize_and_write_pid(pidfile) != 0) {
            fprintf(stderr, "Failed to daemonize/write pidfile\n");
            return 1;
        }
    }

    // Scheduler
    if (scheduler_init() != 0) {
        fprintf(stderr, "Failed to start scheduler worker\n");
        log_close();
        if (use_daemon) remove_pidfile(pidfile);
        return 1;
    }

    // Server
    int result = start_server();

    // Cleanup
    scheduler_shutdown();
    tls_cleanup();
    log_close();
    if (use_daemon) remove_pidfile(pidfile);

    return result;
}