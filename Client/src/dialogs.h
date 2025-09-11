/**
 * Dialogs Header File
 * Function declarations for dialog windows
 */

#ifndef DIALOGS_H
#define DIALOGS_H

#include <gtk/gtk.h>

// Dialog functions
void show_config_dialog(GtkWindow *parent);
void show_credits_dialog(GtkWindow *parent);
void show_message_dialog(GtkWindow *parent, const char *title, const char *message);

// File dialog callback
void on_file_dialog_response(GtkFileDialog *dialog, GAsyncResult *result, gpointer user_data);

#endif // DIALOGS_H