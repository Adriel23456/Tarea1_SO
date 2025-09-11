/**
 * GUI Implementation
 * Main GUI functionality and window creation
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gui.h"
#include "dialogs.h"

// Global app data
static AppData *g_app_data = NULL;

/**
 * Application activation callback
 */
void on_app_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data; // Suppress unused parameter warning
    
    AppData *app_data = g_malloc(sizeof(AppData));
    app_data->app = app;
    app_data->loaded_images = NULL;
    g_app_data = app_data;
    
    create_main_window(app_data);
}

/**
 * Create the main application window
 */
void create_main_window(AppData *app_data) {
    GtkWidget *main_box;
    GtkWidget *header_bar;
    GtkWidget *button_box;
    GtkWidget *list_frame;
    GtkWidget *load_button, *config_button, *send_button, *credits_button, *exit_button;
    
    // Create main window
    app_data->window = gtk_application_window_new(app_data->app);
    gtk_window_set_title(GTK_WINDOW(app_data->window), "Image Processing Client");
    gtk_window_set_default_size(GTK_WINDOW(app_data->window), 800, 600);
    
    // Create header bar for modern look
    header_bar = gtk_header_bar_new();
    gtk_header_bar_set_title_widget(GTK_HEADER_BAR(header_bar), 
                                     gtk_label_new("Image Processing Client"));
    gtk_header_bar_set_show_title_buttons(GTK_HEADER_BAR(header_bar), TRUE);
    gtk_window_set_titlebar(GTK_WINDOW(app_data->window), header_bar);
    
    // Main vertical box
    main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(main_box, 20);
    gtk_widget_set_margin_end(main_box, 20);
    gtk_widget_set_margin_top(main_box, 20);
    gtk_widget_set_margin_bottom(main_box, 20);
    
    // Create button box for actions
    button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    
    // Create buttons with icons and labels
    load_button = gtk_button_new_with_label("Load");
    gtk_widget_add_css_class(load_button, "suggested-action");
    
    config_button = gtk_button_new_with_label("Configuration");
    
    send_button = gtk_button_new_with_label("Send Images");
    gtk_widget_add_css_class(send_button, "suggested-action");
    
    credits_button = gtk_button_new_with_label("Credits");
    
    exit_button = gtk_button_new_with_label("Exit");
    gtk_widget_add_css_class(exit_button, "destructive-action");
    
    // Add buttons to button box
    gtk_box_append(GTK_BOX(button_box), load_button);
    gtk_box_append(GTK_BOX(button_box), config_button);
    gtk_box_append(GTK_BOX(button_box), send_button);
    gtk_box_append(GTK_BOX(button_box), credits_button);
    gtk_box_append(GTK_BOX(button_box), exit_button);
    
    // Create frame for image list
    list_frame = gtk_frame_new("Loaded Images");
    gtk_widget_set_vexpand(list_frame, TRUE);
    
    // Create scrolled window for image list
    app_data->scrolled_window = gtk_scrolled_window_new();
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(app_data->scrolled_window),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(app_data->scrolled_window, TRUE);
    
    // Create list box for images
    app_data->image_list_box = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(app_data->image_list_box), 
                                     GTK_SELECTION_SINGLE);
    gtk_widget_add_css_class(app_data->image_list_box, "boxed-list");
    
    // Add placeholder when no images
    GtkWidget *placeholder = gtk_label_new("No images loaded\nClick 'Load' to add images");
    gtk_widget_set_opacity(placeholder, 0.5);
    gtk_list_box_set_placeholder(GTK_LIST_BOX(app_data->image_list_box), placeholder);
    
    // Add list box to scrolled window
    gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(app_data->scrolled_window), 
                                  app_data->image_list_box);
    gtk_frame_set_child(GTK_FRAME(list_frame), app_data->scrolled_window);
    
    // Connect button signals
    g_signal_connect(load_button, "clicked", G_CALLBACK(on_load_button_clicked), app_data);
    g_signal_connect(config_button, "clicked", G_CALLBACK(on_config_button_clicked), app_data);
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_button_clicked), app_data);
    g_signal_connect(credits_button, "clicked", G_CALLBACK(on_credits_button_clicked), app_data);
    g_signal_connect(exit_button, "clicked", G_CALLBACK(on_exit_button_clicked), app_data);
    
    // Add widgets to main box
    gtk_box_append(GTK_BOX(main_box), button_box);
    gtk_box_append(GTK_BOX(main_box), list_frame);
    
    // Set main box as window child
    gtk_window_set_child(GTK_WINDOW(app_data->window), main_box);
    
    // Apply CSS for modern styling
    GtkCssProvider *css_provider = gtk_css_provider_new();
    const char *css = 
        ".boxed-list { "
        "  background-color: #ffffff; "
        "  border-radius: 8px; "
        "  border: 1px solid #d0d0d0; "
        "} "
        ".image-row { "
        "  padding: 10px; "
        "  border-bottom: 1px solid #e0e0e0; "
        "  background-color: #ffffff; "
        "} "
        ".image-row:hover { "
        "  background-color: #f0f0f0; "
        "} "
        ".image-row label { "
        "  color: #2c3e50; "
        "  font-weight: 500; "
        "} "
        ".image-row label:last-child { "
        "  color: #7f8c8d; "
        "  font-weight: normal; "
        "} "
        "list row { "
        "  background-color: transparent; "
        "} "
        "list row:selected { "
        "  background-color: #3498db; "
        "} "
        "list row:selected { "
        "  background-color: #3498db; "
        "} "
        "list row:selected label { "
        "  color: #ff0000; "
        "} "
        ;
    
    gtk_css_provider_load_from_string(css_provider, css);
    gtk_style_context_add_provider_for_display(
        gtk_widget_get_display(app_data->window),
        GTK_STYLE_PROVIDER(css_provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    
    // Show the window
    gtk_window_present(GTK_WINDOW(app_data->window));
}

/**
 * Load button callback - Opens file chooser for image selection
 */
void on_load_button_clicked(GtkWidget *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    AppData *app_data = (AppData *)user_data;
    GtkFileDialog *dialog;
    GtkFileFilter *filter;
    
    // Create file dialog
    dialog = gtk_file_dialog_new();
    gtk_file_dialog_set_title(dialog, "Select Image Files (Use Ctrl/Shift for multiple)");
    gtk_file_dialog_set_modal(dialog, TRUE);
    
    // Create and set file filter for images
    filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Image Files");
    gtk_file_filter_add_pattern(filter, "*.jpg");
    gtk_file_filter_add_pattern(filter, "*.jpeg");
    gtk_file_filter_add_pattern(filter, "*.png");
    gtk_file_filter_add_pattern(filter, "*.gif");
    gtk_file_filter_add_pattern(filter, "*.JPG");
    gtk_file_filter_add_pattern(filter, "*.JPEG");
    gtk_file_filter_add_pattern(filter, "*.PNG");
    gtk_file_filter_add_pattern(filter, "*.GIF");
    
    GListStore *filters = g_list_store_new(GTK_TYPE_FILE_FILTER);
    g_list_store_append(filters, filter);
    gtk_file_dialog_set_filters(dialog, G_LIST_MODEL(filters));
    
    // Open file dialog for MULTIPLE selection
    gtk_file_dialog_open_multiple(dialog, GTK_WINDOW(app_data->window), NULL,
                                 (GAsyncReadyCallback)on_file_dialog_multiple_response, app_data);
}

/**
 * Configuration button callback - Opens configuration editor
 */
void on_config_button_clicked(GtkWidget *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    AppData *app_data = (AppData *)user_data;
    show_config_dialog(GTK_WINDOW(app_data->window));
}

/**
 * Send images button callback - Will send images to server (placeholder for now)
 */
void on_send_button_clicked(GtkWidget *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    AppData *app_data = (AppData *)user_data;
    
    if (app_data->loaded_images == NULL) {
        show_message_dialog(GTK_WINDOW(app_data->window), 
                           "No Images", 
                           "Please load some images first!");
    } else {
        show_message_dialog(GTK_WINDOW(app_data->window), 
                           "Send Images", 
                           "Image sending functionality will be implemented soon!");
    }
}

/**
 * Credits button callback - Shows credits dialog
 */
void on_credits_button_clicked(GtkWidget *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    AppData *app_data = (AppData *)user_data;
    show_credits_dialog(GTK_WINDOW(app_data->window));
}

/**
 * Exit button callback - Closes the application
 */
void on_exit_button_clicked(GtkWidget *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    AppData *app_data = (AppData *)user_data;
    
    // Clean up loaded images list
    if (app_data->loaded_images) {
        g_slist_free_full(app_data->loaded_images, g_free);
    }
    
    // Close the window
    gtk_window_close(GTK_WINDOW(app_data->window));
}

/**
 * Add an image to the list display
 */
void add_image_to_list(AppData *app_data, const char *filepath) {
    GtkWidget *row_box;
    GtkWidget *icon_label;
    GtkWidget *label;
    GtkWidget *size_label;
    char *basename;
    char size_text[64];
    
    // Get file info
    GFile *file = g_file_new_for_path(filepath);
    GFileInfo *info = g_file_query_info(file, 
                                        G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                        G_FILE_QUERY_INFO_NONE,
                                        NULL, NULL);
    
    // Create row box
    row_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_add_css_class(row_box, "image-row");
    gtk_widget_set_margin_start(row_box, 10);
    gtk_widget_set_margin_end(row_box, 10);
    gtk_widget_set_margin_top(row_box, 5);
    gtk_widget_set_margin_bottom(row_box, 5);
    
    // Add an icon for the image
    icon_label = gtk_label_new("🖼️");
    gtk_widget_set_margin_end(icon_label, 5);
    
    // Get basename for display
    basename = g_path_get_basename(filepath);
    
    // Create labels
    label = gtk_label_new(basename);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_widget_set_tooltip_text(label, filepath); // Show full path on hover
    
    if (info) {
        goffset size = g_file_info_get_size(info);
        if (size < 1024) {
            g_snprintf(size_text, sizeof(size_text), "%ld B", (long)size);
        } else if (size < 1024 * 1024) {
            g_snprintf(size_text, sizeof(size_text), "%.1f KB", size / 1024.0);
        } else {
            g_snprintf(size_text, sizeof(size_text), "%.1f MB", size / (1024.0 * 1024.0));
        }
        g_object_unref(info);
    } else {
        g_snprintf(size_text, sizeof(size_text), "Unknown size");
    }
    
    size_label = gtk_label_new(size_text);
    gtk_widget_set_opacity(size_label, 0.7);
    
    // Add widgets to row box
    gtk_box_append(GTK_BOX(row_box), icon_label);
    gtk_box_append(GTK_BOX(row_box), label);
    gtk_box_append(GTK_BOX(row_box), size_label);
    
    // Add row to list box
    gtk_list_box_append(GTK_LIST_BOX(app_data->image_list_box), row_box);
    
    // Store the file path
    app_data->loaded_images = g_slist_append(app_data->loaded_images, g_strdup(filepath));
    
    // Clean up
    g_free(basename);
    g_object_unref(file);
}

/**
 * Clear all images from the list
 */
void clear_image_list(AppData *app_data) {
    GtkWidget *child;
    
    // Remove all children from list box
    while ((child = gtk_widget_get_first_child(app_data->image_list_box)) != NULL) {
        gtk_list_box_remove(GTK_LIST_BOX(app_data->image_list_box), child);
    }
    
    // Clear the loaded images list
    if (app_data->loaded_images) {
        g_slist_free_full(app_data->loaded_images, g_free);
        app_data->loaded_images = NULL;
    }
}