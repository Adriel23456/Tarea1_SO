/**
 * Image Processing Client
 * Main application entry point
 */

#include <gtk/gtk.h>
#include "gui.h"

/*
 * main
 * ----
 * Program entry point. Initializes a GTK application, hooks the
 * activation handler and runs the application main loop.
 */
int main(int argc, char **argv) {
    GtkApplication *app;
    int status;
    
    // Create new GTK application
    app = gtk_application_new("com.imageprocessing.client", G_APPLICATION_DEFAULT_FLAGS);
    
    // Connect the activate signal to our GUI initialization
    g_signal_connect(app, "activate", G_CALLBACK(on_app_activate), NULL);
    
    // Run the application
    status = g_application_run(G_APPLICATION(app), argc, argv);
    
    // Clean up
    g_object_unref(app);
    
    return status;
}