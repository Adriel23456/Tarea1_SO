/* src/daemon.h */
#ifndef DAEMON_H
#define DAEMON_H
int daemonize_and_write_pid(const char* pidfile);
int remove_pidfile(const char* pidfile);
#endif