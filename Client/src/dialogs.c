/**
 * Dialogs Implementation
 * Dialog windows for configuration, credits, and messages
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "dialogs.h"
#include "gui.h"

/**
 * Configuration dialog - Edit connection.json
 */
void show_config_dialog(GtkWindow *parent) {
    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *scrolled_window;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    GtkWidget *save_button;
    char *config_content = NULL;
    gsize length;
    GError *error = NULL;
    
    // Create dialog
    dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Server Configuration");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    
    // Get content area
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    // Create scrolled window
    scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    
    // Create text view
    text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(text_view), TRUE);
    gtk_widget_set_margin_start(text_view, 10);
    gtk_widget_set_margin_end(text_view, 10);
    gtk_widget_set_margin_top(text_view, 10);
    gtk_widget_set_margin_bottom(text_view, 10);
    
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    
    // Try to load existing configuration
    if (g_file_get_contents("assets/connection.json", &config_content, &length, &error)) {
        gtk_text_buffer_set_text(buffer, config_content, -1);
        g_free(config_content);
    } else {
        // Create default configuration
        const char *default_config = 
            "{\n"
            "  \"server\": {\n"
            "    \"host\": \"localhost\",\n"
            "    \"port\": 1717,\n"
            "    \"protocol\": \"http\"\n"
            "  },\n"
            "  \"client\": {\n"
            "    \"timeout\": 30,\n"
            "    \"max_retries\": 3\n"
            "  }\n"
            "}";
        gtk_text_buffer_set_text(buffer, default_config, -1);
    }
    
    // Add text view to scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), text_view);
    
    // Add scrolled window to content area
    gtk_box_append(GTK_BOX(content_area), scrolled_window);
    
    // Add dialog buttons
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Cancel", GTK_RESPONSE_CANCEL);
    save_button = gtk_dialog_add_button(GTK_DIALOG(dialog), "Save", GTK_RESPONSE_ACCEPT);
    gtk_widget_add_css_class(save_button, "suggested-action");
    
    // Store text view reference for save operation
    g_object_set_data(G_OBJECT(dialog), "text_view", text_view);
    
    // Connect response signal
    g_signal_connect(dialog, "response", G_CALLBACK(on_config_dialog_response), NULL);
    
    // Show dialog
    gtk_window_present(GTK_WINDOW(dialog));
}

/**
 * Configuration dialog response handler
 */
static void on_config_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
    if (response_id == GTK_RESPONSE_ACCEPT) {
        GtkWidget *text_view = g_object_get_data(G_OBJECT(dialog), "text_view");
        GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
        GtkTextIter start, end;
        char *text;
        
        // Get text from buffer
        gtk_text_buffer_get_bounds(buffer, &start, &end);
        text = gtk_text_buffer_get_text(buffer, &start, &end, FALSE);
        
        // Create assets directory if it doesn't exist
        g_mkdir_with_parents("assets", 0755);
        
        // Save to file
        GError *error = NULL;
        if (!g_file_set_contents("assets/connection.json", text, -1, &error)) {
            g_printerr("Error saving configuration: %s\n", error->message);
            g_error_free(error);
        } else {
            g_print("Configuration saved successfully\n");
        }
        
        g_free(text);
    }
    
    gtk_window_destroy(GTK_WINDOW(dialog));
}

/**
 * Credits dialog - Display credits.txt
 */
void show_credits_dialog(GtkWindow *parent) {
    GtkWidget *dialog;
    GtkWidget *content_area;
    GtkWidget *scrolled_window;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    char *credits_content = NULL;
    gsize length;
    GError *error = NULL;
    
    // Create dialog
    dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Credits");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    
    // Get content area
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
    
    // Create scrolled window
    scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    
    // Create text view
    text_view = gtk_text_view_new();
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_view), GTK_WRAP_WORD);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(text_view), FALSE);
    gtk_widget_set_margin_start(text_view, 10);
    gtk_widget_set_margin_end(text_view, 10);
    gtk_widget_set_margin_top(text_view, 10);
    gtk_widget_set_margin_bottom(text_view, 10);
    
    buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text_view));
    
    // Try to load credits file
    if (g_file_get_contents("assets/credits.txt", &credits_content, &length, &error)) {
        gtk_text_buffer_set_text(buffer, credits_content, -1);
        g_free(credits_content);
    } else {
        // Default credits text
        const char *default_credits = 
            "IMAGE PROCESSING CLIENT\n"
            "=======================\n\n"
            "Version 1.0.0\n\n"
            "Developed for Systems Operations Course\n\n"
            "Features:\n"
            "- Image loading and management\n"
            "- Server configuration\n"
            "- Batch image processing\n"
            "- Histogram equalization support\n"
            "- Color-based classification\n\n"
            "Technologies:\n"
            "- GTK4 for GUI\n"
            "- C Programming Language\n"
            "- HTTP/TCP Protocol\n\n"
            "© 2024 - All Rights Reserved";
        gtk_text_buffer_set_text(buffer, default_credits, -1);
    }
    
    // Add text view to scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), text_view);
    
    // Add scrolled window to content area
    gtk_box_append(GTK_BOX(content_area), scrolled_window);
    
    // Add close button
    gtk_dialog_add_button(GTK_DIALOG(dialog), "Close", GTK_RESPONSE_CLOSE);
    
    // Connect response signal to close dialog
    g_signal_connect(dialog, "response", G_CALLBACK(gtk_window_destroy), NULL);
    
    // Show dialog
    gtk_window_present(GTK_WINDOW(dialog));
}

/**
 * Simple message dialog
 */
void show_message_dialog(GtkWindow *parent, const char *title, const char *message) {
    GtkAlertDialog *alert;
    
    alert = gtk_alert_dialog_new("%s", message);
    gtk_alert_dialog_show(alert, parent);
    g_object_unref(alert);
}

/**
 * File dialog response handler
 */
void on_file_dialog_response(GtkFileDialog *dialog, GAsyncResult *result, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    GFile *file;
    GError *error = NULL;
    
    file = gtk_file_dialog_open_finish(dialog, result, &error);
    
    if (file) {
        char *filepath = g_file_get_path(file);
        
        // Check if file is a valid image format
        const char *valid_extensions[] = {".jpg", ".jpeg", ".png", ".gif", 
                                         ".JPG", ".JPEG", ".PNG", ".GIF", NULL};
        gboolean valid = FALSE;
        
        for (int i = 0; valid_extensions[i] != NULL; i++) {
            if (g_str_has_suffix(filepath, valid_extensions[i])) {
                valid = TRUE;
                break;
            }
        }
        
        if (valid) {
            // Add image to list
            add_image_to_list(app_data, filepath);
            g_print("Loaded image: %s\n", filepath);
        } else {
            show_message_dialog(GTK_WINDOW(app_data->window), 
                              "Invalid Format", 
                              "Please select a valid image file (jpg, jpeg, png, gif)");
        }
        
        g_free(filepath);
        g_object_unref(file);
    } else if (error) {
        if (error->code != G_IO_ERROR_CANCELLED) {
            g_printerr("Error opening file: %s\n", error->message);
        }
        g_error_free(error);
    }
    
    g_object_unref(dialog);
}

// Add missing static declaration
static void on_config_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data);