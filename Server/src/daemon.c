/* src/daemon.c */
#include "daemon.h"

#include <unistd.h>     // fork, chdir, setsid, dup2, close
#include <sys/types.h>
#include <sys/stat.h>   // umask
#include <fcntl.h>      // open
#include <stdio.h>      // snprintf, perror
#include <stdlib.h>     // _exit
#include <errno.h>
#include <string.h>

// Escribe el PID del proceso actual en pidfile.
// Devuelve 0 en éxito, -1 en error (y deja errno configurado).
static int write_pidfile(const char* pidfile) {
    if (!pidfile) return 0;

    int fd = open(pidfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        // Si falla por directorio inexistente, se deja error a quien llama
        return -1;
    }

    char buf[64];
    int len = snprintf(buf, sizeof(buf), "%ld\n", (long)getpid());
    if (len < 0 || len >= (int)sizeof(buf)) {
        (void)close(fd);
        errno = EOVERFLOW;
        return -1;
    }

    ssize_t w = write(fd, buf, (size_t)len);
    if (w != len) {
        int saved = errno;
        (void)close(fd);
        errno = (w < 0) ? saved : EIO;
        return -1;
    }

    if (close(fd) != 0) {
        return -1;
    }
    return 0;
}

int daemonize_and_write_pid(const char* pidfile) {
    // fork #1
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid > 0) {
        _exit(0); // padre sale
    }

    // nueva sesión
    if (setsid() < 0) {
        perror("setsid");
        return -1;
    }

    // fork #2
    pid = fork();
    if (pid < 0) {
        perror("fork(2)");
        return -1;
    }
    if (pid > 0) {
        _exit(0); // líder de sesión sale
    }

    // permisos y directorio de trabajo
    umask(0);
    if (chdir("/") != 0) {
        perror("chdir(/)");
        return -1;   // <-- esto elimina el warning
    }

    // redirigir stdin/stdout/stderr a /dev/null
    int fd = open("/dev/null", O_RDWR);
    if (fd >= 0) {
        if (dup2(fd, STDIN_FILENO)  < 0) { perror("dup2 stdin");  /* seguimos */ }
        if (dup2(fd, STDOUT_FILENO) < 0) { perror("dup2 stdout"); /* seguimos */ }
        if (dup2(fd, STDERR_FILENO) < 0) { perror("dup2 stderr"); /* seguimos */ }
        if (fd > STDERR_FILENO) (void)close(fd);
    } else {
        perror("open(/dev/null)");
        // no abortamos: en la práctica el demonio aún puede correr
    }

    // escribir PIDFile (si se pidió)
    if (pidfile && write_pidfile(pidfile) != 0) {
        perror("write_pidfile");
        return -1;
    }

    return 0;
}

int remove_pidfile(const char* pidfile) {
    if (!pidfile) return 0;
    // No pasa nada si no existe
    if (unlink(pidfile) != 0 && errno != ENOENT) {
        // No lo tratamos como fatal
        return -1;
    }
    return 0;
}