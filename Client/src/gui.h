/**
 * GUI Header File
 * Contains structures and function declarations for the GUI
 */

#ifndef GUI_H
#define GUI_H

#include <gtk/gtk.h>

// Application data structure
typedef struct {
    GtkApplication *app;
    GtkWidget *window;
    GtkWidget *image_list_box;
    GtkWidget *scrolled_window;
    GListStore *image_store;
    GSList *loaded_images;  // List of loaded image paths
} AppData;

// Main GUI functions
void on_app_activate(GtkApplication *app, gpointer user_data);
void create_main_window(AppData *app_data);

// Button callbacks
void on_load_button_clicked(GtkWidget *button, gpointer user_data);
void on_config_button_clicked(GtkWidget *button, gpointer user_data);
void on_send_button_clicked(GtkWidget *button, gpointer user_data);
void on_credits_button_clicked(GtkWidget *button, gpointer user_data);
void on_exit_button_clicked(GtkWidget *button, gpointer user_data);

// Utility functions
void add_image_to_list(AppData *app_data, const char *filepath);
void clear_image_list(AppData *app_data);

#endif // GUI_H