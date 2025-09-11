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

// Forward declarations for static functions
static void on_config_save_clicked(GtkWidget *button, gpointer user_data);
static void on_credits_close_clicked(GtkWidget *button, gpointer user_data);

/**
 * Configuration dialog - Edit connection.json
 */
void show_config_dialog(GtkWindow *parent) {
    GtkWidget *dialog;
    GtkWidget *box;
    GtkWidget *scrolled_window;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    GtkWidget *button_box;
    GtkWidget *save_button;
    GtkWidget *cancel_button;
    GtkWidget *header_bar;
    char *config_content = NULL;
    gsize length;
    GError *error = NULL;
    
    // Create window instead of deprecated dialog
    dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Server Configuration");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    
    // Create header bar with title
    header_bar = gtk_header_bar_new();
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header_bar), 
                                     gtk_label_new("Server Configuration"));
    gtk_window_set_titlebar(GTK_WINDOW(dialog), header_bar);
    
    // Create main box
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 10);
    gtk_widget_set_margin_bottom(box, 10);
    
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
        "    \"chunk_size\": 65536,\n"
        "    \"connect_timeout\": 10,\n"
        "    \"max_retries\": 3,\n"
        "    \"retry_backoff_ms\": 500\n"
        "  }\n"
        "}";
        gtk_text_buffer_set_text(buffer, default_config, -1);
        if (error) {
            g_error_free(error);
        }
    }
    
    // Add text view to scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), text_view);
    
    // Create button box
    button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_END);
    
    // Create buttons
    cancel_button = gtk_button_new_with_label("Cancel");
    save_button = gtk_button_new_with_label("Save");
    gtk_widget_add_css_class(save_button, "suggested-action");
    
    // Add buttons to button box
    gtk_box_append(GTK_BOX(button_box), cancel_button);
    gtk_box_append(GTK_BOX(button_box), save_button);
    
    // Add widgets to main box
    gtk_box_append(GTK_BOX(box), scrolled_window);
    gtk_box_append(GTK_BOX(box), button_box);
    
    // Set main box as window child
    gtk_window_set_child(GTK_WINDOW(dialog), box);
    
    // Store text view reference for save operation
    g_object_set_data(G_OBJECT(save_button), "text_view", text_view);
    g_object_set_data(G_OBJECT(save_button), "dialog", dialog);
    
    // Connect button signals
    g_signal_connect_swapped(cancel_button, "clicked", 
                            G_CALLBACK(gtk_window_destroy), dialog);
    g_signal_connect(save_button, "clicked", 
                    G_CALLBACK(on_config_save_clicked), NULL);
    
    // Show dialog
    gtk_window_present(GTK_WINDOW(dialog));
}

/**
 * Configuration save button handler
 */
static void on_config_save_clicked(GtkWidget *button, gpointer user_data) {
    (void)user_data; // Suppress unused parameter warning
    
    GtkWidget *text_view = g_object_get_data(G_OBJECT(button), "text_view");
    GtkWidget *dialog = g_object_get_data(G_OBJECT(button), "dialog");
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
    
    // Close dialog
    gtk_window_destroy(GTK_WINDOW(dialog));
}

/**
 * Credits dialog - Display credits.txt
 */
void show_credits_dialog(GtkWindow *parent) {
    GtkWidget *dialog;
    GtkWidget *box;
    GtkWidget *scrolled_window;
    GtkWidget *text_view;
    GtkTextBuffer *buffer;
    GtkWidget *close_button;
    GtkWidget *header_bar;
    char *credits_content = NULL;
    gsize length;
    GError *error = NULL;
    
    // Create window instead of deprecated dialog
    dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog), "Credits");
    gtk_window_set_default_size(GTK_WINDOW(dialog), 500, 400);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    
    // Create header bar with title
    header_bar = gtk_header_bar_new();
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header_bar), 
                                     gtk_label_new("Credits"));
    gtk_window_set_titlebar(GTK_WINDOW(dialog), header_bar);
    
    // Create main box
    box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(box, 10);
    gtk_widget_set_margin_end(box, 10);
    gtk_widget_set_margin_top(box, 10);
    gtk_widget_set_margin_bottom(box, 10);
    
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
        if (error) {
            g_error_free(error);
        }
    }
    
    // Add text view to scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled_window), text_view);
    
    // Create close button
    close_button = gtk_button_new_with_label("Close");
    gtk_widget_set_halign(close_button, GTK_ALIGN_CENTER);
    
    // Add widgets to main box
    gtk_box_append(GTK_BOX(box), scrolled_window);
    gtk_box_append(GTK_BOX(box), close_button);
    
    // Set main box as window child
    gtk_window_set_child(GTK_WINDOW(dialog), box);
    
    // Store dialog reference for close operation
    g_object_set_data(G_OBJECT(close_button), "dialog", dialog);
    
    // Connect close button signal
    g_signal_connect(close_button, "clicked", 
                    G_CALLBACK(on_credits_close_clicked), NULL);
    
    // Show dialog
    gtk_window_present(GTK_WINDOW(dialog));
}

/**
 * Credits close button handler
 */
static void on_credits_close_clicked(GtkWidget *button, gpointer user_data) {
    (void)user_data; // Suppress unused parameter warning
    
    GtkWidget *dialog = g_object_get_data(G_OBJECT(button), "dialog");
    gtk_window_destroy(GTK_WINDOW(dialog));
}

/**
 * Simple message dialog
 */
void show_message_dialog(GtkWindow *parent, const char *title, const char *message) {
    (void)title; // Title not used in GtkAlertDialog, suppress warning
    
    GtkAlertDialog *alert;
    
    alert = gtk_alert_dialog_new("%s", message);
    gtk_alert_dialog_show(alert, parent);
    g_object_unref(alert);
}

/**
 * File dialog response handler (single file - kept for compatibility)
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

/**
 * File dialog response handler for multiple files
 */
void on_file_dialog_multiple_response(GtkFileDialog *dialog, GAsyncResult *result, gpointer user_data) {
    AppData *app_data = (AppData *)user_data;
    GListModel *files;
    GError *error = NULL;
    guint n_files;
    int valid_count = 0;
    int invalid_count = 0;
    
    files = gtk_file_dialog_open_multiple_finish(dialog, result, &error);
    
    if (files) {
        n_files = g_list_model_get_n_items(files);
        
        // Process each selected file
        for (guint i = 0; i < n_files; i++) {
            GFile *file = G_FILE(g_list_model_get_item(files, i));
            char *filepath = g_file_get_path(file);
            
            // Check if file is a valid image format
            const char *valid_extensions[] = {".jpg", ".jpeg", ".png", ".gif", 
                                             ".JPG", ".JPEG", ".PNG", ".GIF", NULL};
            gboolean valid = FALSE;
            
            for (int j = 0; valid_extensions[j] != NULL; j++) {
                if (g_str_has_suffix(filepath, valid_extensions[j])) {
                    valid = TRUE;
                    break;
                }
            }
            
            if (valid) {
                // Add image to list
                add_image_to_list(app_data, filepath);
                g_print("Loaded image: %s\n", filepath);
                valid_count++;
            } else {
                g_printerr("Skipped non-image file: %s\n", filepath);
                invalid_count++;
            }
            
            g_free(filepath);
            g_object_unref(file);
        }
        
        // Show summary message if there were any issues
        if (invalid_count > 0) {
            char message[256];
            g_snprintf(message, sizeof(message), 
                      "Loaded %d image(s) successfully.\n%d file(s) were skipped (not valid image formats).",
                      valid_count, invalid_count);
            show_message_dialog(GTK_WINDOW(app_data->window), "Load Summary", message);
        } else if (valid_count > 1) {
            char message[128];
            g_snprintf(message, sizeof(message), 
                      "Successfully loaded %d images!", valid_count);
            show_message_dialog(GTK_WINDOW(app_data->window), "Success", message);
        }
        
        g_object_unref(files);
    } else if (error) {
        if (error->code != G_IO_ERROR_CANCELLED) {
            g_printerr("Error opening files: %s\n", error->message);
        }
        g_error_free(error);
    }
    
    g_object_unref(dialog);
}