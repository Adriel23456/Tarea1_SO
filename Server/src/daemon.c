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
// Returns 0 on success, -1 on error (errno is set).
/*
 * write_pidfile
 * -------------
 * Write the current process PID into `pidfile`.
 * Parameters:
 *  - pidfile: path to write the PID (if NULL, function is a no-op).
 * Returns:
 *  - 0 on success, -1 on failure (errno is set accordingly).
 */
static int write_pidfile(const char* pidfile) {
    if (!pidfile) return 0;

    int fd = open(pidfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        // If it fails due to a missing directory, leave the error for the caller
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

/*
 * daemonize_and_write_pid
 * -----------------------
 * Perform a standard double-fork daemonization sequence and write the
 * new daemon PID into `pidfile` if provided.
 * Parameters:
 *  - pidfile: path to write the child PID, or NULL to skip.
 * Returns:
 *  - 0 on success, -1 on failure.
 */
int daemonize_and_write_pid(const char* pidfile) {
    // fork #1
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid > 0) {
        _exit(0); // parent exits
    }

    // create a new session
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
    _exit(0); // session leader exits
    }

    // file mode mask and working directory
    umask(0);
    if (chdir("/") != 0) {
        perror("chdir(/)");
        return -1;   // <-- this removes the warning
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
    // do not abort: the daemon can continue running in practice
    }

    // write PID file (if requested)
    if (pidfile && write_pidfile(pidfile) != 0) {
        perror("write_pidfile");
        return -1;
    }

    return 0;
}

/*
 * remove_pidfile
 * --------------
 * Remove the PID file if it exists. Non-fatal if file is absent.
 * Returns 0 on success, -1 if unlink failed for reasons other than ENOENT.
 */
int remove_pidfile(const char* pidfile) {
    if (!pidfile) return 0;
    // It's fine if the file does not exist
    if (unlink(pidfile) != 0 && errno != ENOENT) {
        // Not treated as fatal
        return -1;
    }
    return 0;
}