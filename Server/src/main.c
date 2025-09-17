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

// Signals: handlers
static const char* g_pidfile = NULL;

/*
 * handle_sigterm
 * --------------
 * Signal handler for termination signals. Requests server shutdown.
 */
static void handle_sigterm(int sig) {
    (void)sig;
    server_request_shutdown();
}

/*
 * handle_sighup
 * -------------
 * Signal handler for SIGHUP: marks configuration reload request.
 */
static void handle_sighup(int sig) {
    (void)sig;
    g_reload = 1;
}

/*
 * install_signal_handlers
 * -----------------------
 * Install process signal handlers used by the server (SIGTERM, SIGINT,
 * SIGHUP). This is called once on startup.
 */
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

/*
 * usage
 * -----
 * Print basic command-line usage help to stderr.
 */
static void usage(const char* prog) {
    fprintf(stderr,
        "Usage: %s [--config <path>] [--daemon] [--pidfile <path>] [--foreground]\n"
        "       --config    Path to config.json (default: assets/config.json)\n"
        "       --daemon    Double-fork + PIDFile (classic daemon mode)\n"
        "       --pidfile   Path for PIDFile (default: /run/ImageService.pid)\n"
        "       --foreground (default) run in foreground (good for systemd)\n",
        prog);
}

/*
 * main
 * ----
 * Application entry point. Responsibilities:
 *  - parse command line options
 *  - load configuration and create required directories
 *  - initialize logging, scheduler and server
 *  - optionally daemonize and write pidfile
 */
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

    // Load config
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

    // Signals
    install_signal_handlers();

    // Classic daemon mode (optional)
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