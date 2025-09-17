/**
 * GUI Implementation
 * Main GUI functionality and window creation
 */

#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <json-c/json.h>
#include "gui.h"
#include "dialogs.h"
#include "network.h"
#include "protocol.h"

// Structure for progress dialog
typedef struct {
    GtkWidget *window;
    GtkWidget *progress_bar;
    GtkWidget *label;
    ProcessingType proc_type;
    GSList *image_list;
} SendDialogData;

// Progress callback
/*
 * send_progress_callback
 * ----------------------
 * Simple progress callback used by the sending thread to report
 * human-readable progress messages. This implementation prints to
 * stdout; the GUI could be updated instead if needed.
 */
static void send_progress_callback(const char* message, double progress) {
    printf("%s (%.0f%%)\n", message, progress * 100);
}

// NO SE JAJA
/*
 * send_thread_func
 * ----------------
 * Thread function that reads `assets/connection.json` to populate a
 * `NetConfig` and then calls `send_all_images` to transmit images.
 * The thread returns an integer status encoded in a gpointer.
 */
static gpointer send_thread_func(gpointer data) {
    SendDialogData *dialog_data = (SendDialogData*)data;

    // Config por defecto con buffers propios
    NetConfig cfg;
    memset(&cfg, 0, sizeof(cfg));
    g_strlcpy(cfg.host, "localhost", sizeof(cfg.host));
    cfg.port = DEFAULT_PORT;
    g_strlcpy(cfg.protocol, "http", sizeof(cfg.protocol));
    cfg.chunk_size = DEFAULT_CHUNK_SIZE;
    cfg.connect_timeout = 10;
    cfg.max_retries = 3;
    cfg.retry_backoff_ms = 500;

    // Leer assets/connection.json
    FILE* fp = fopen("assets/connection.json", "r");
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);

        char* json_str = (char*)malloc((size_t)size + 1);
        if (json_str) {
            size_t rd = fread(json_str, 1, (size_t)size, fp);
            json_str[rd] = '\0';
            struct json_object* root = json_tokener_parse(json_str);
            if (root) {
                struct json_object *server_obj = NULL, *client_obj = NULL;

                if (json_object_object_get_ex(root, "server", &server_obj)) {
                    struct json_object *host_obj=NULL, *port_obj=NULL, *proto_obj=NULL;
                    if (json_object_object_get_ex(server_obj, "host", &host_obj)) {
                        const char* s = json_object_get_string(host_obj);
                        if (s) g_strlcpy(cfg.host, s, sizeof(cfg.host));
                    }
                    if (json_object_object_get_ex(server_obj, "port", &port_obj)) {
                        cfg.port = json_object_get_int(port_obj);
                    }
                    if (json_object_object_get_ex(server_obj, "protocol", &proto_obj)) {
                        const char* s = json_object_get_string(proto_obj);
                        if (s) g_strlcpy(cfg.protocol, s, sizeof(cfg.protocol));
                    }
                }

                if (json_object_object_get_ex(root, "client", &client_obj)) {
                    struct json_object *chunk_obj=NULL, *cto_obj=NULL, *mr_obj=NULL, *rb_obj=NULL;
                    if (json_object_object_get_ex(client_obj, "chunk_size", &chunk_obj))
                        cfg.chunk_size = json_object_get_int(chunk_obj);
                    if (json_object_object_get_ex(client_obj, "connect_timeout", &cto_obj))
                        cfg.connect_timeout = json_object_get_int(cto_obj);
                    if (json_object_object_get_ex(client_obj, "max_retries", &mr_obj))
                        cfg.max_retries = json_object_get_int(mr_obj);
                    if (json_object_object_get_ex(client_obj, "retry_backoff_ms", &rb_obj))
                        cfg.retry_backoff_ms = json_object_get_int(rb_obj);
                }

                json_object_put(root);
            }
            free(json_str);
        }
        fclose(fp);
    }

    int result = send_all_images(dialog_data->image_list, &cfg,
                                 dialog_data->proc_type, send_progress_callback);
    return GINT_TO_POINTER(result);
}

// Global app data
static AppData *g_app_data = NULL;

/*
 * on_app_activate
 * ----------------
 * GTK application activation callback. Allocates and initializes the
 * global `AppData` and creates the main window.
 */
void on_app_activate(GtkApplication *app, gpointer user_data) {
    (void)user_data; // Suppress unused parameter warning
    
    AppData *app_data = g_malloc(sizeof(AppData));
    app_data->app = app;
    app_data->loaded_images = NULL;
    g_app_data = app_data;
    
    create_main_window(app_data);
}

/*
 * create_main_window
 * ------------------
 * Build and present the application's main window, wire callbacks to
 * UI actions and initialize widgets used to display loaded images.
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

/*
 * on_load_button_clicked
 * ----------------------
 * Open a file chooser dialog (multi-select) to let the user pick
 * image files. The selected files are handled asynchronously by
 * `on_file_dialog_multiple_response`.
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

/*
 * on_config_button_clicked
 * ------------------------
 * Show the configuration editor dialog when the Configuration
 * button is pressed.
 */
void on_config_button_clicked(GtkWidget *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    AppData *app_data = (AppData *)user_data;
    show_config_dialog(GTK_WINDOW(app_data->window));
}

/*
 * on_send_button_clicked
 * ----------------------
 * Present a modal dialog to select processing type, then spawn a
 * worker thread to send images to the server. Shows a progress
 * dialog while the transfer runs and displays a completion message.
 */
void on_send_button_clicked(GtkWidget *button, gpointer user_data) {
    (void)button;
    AppData *app_data = (AppData *)user_data;

    if (app_data->loaded_images == NULL) {
        show_message_dialog(GTK_WINDOW(app_data->window),
                            "No Images",
                            "Please load some images first!");
        return;
    }

    /* --- Create modal window to choose processing type --- */
    GtkWidget *dialog_win = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(dialog_win), "Select Processing Type");
    gtk_window_set_transient_for(GTK_WINDOW(dialog_win), GTK_WINDOW(app_data->window));
    gtk_window_set_modal(GTK_WINDOW(dialog_win), TRUE);
    gtk_window_set_default_size(GTK_WINDOW(dialog_win), 420, 180);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(vbox, 20);
    gtk_widget_set_margin_end(vbox, 20);
    gtk_widget_set_margin_top(vbox, 20);
    gtk_widget_set_margin_bottom(vbox, 20);

    GtkWidget *label = gtk_label_new("Select processing type for images:");
    gtk_box_append(GTK_BOX(vbox), label);

    /* Radio buttons en GTK4 = CheckButtons en el mismo grupo */
    GtkWidget *radio_hist = gtk_check_button_new_with_label("Histogram Equalization");
    gtk_check_button_set_active(GTK_CHECK_BUTTON(radio_hist), TRUE);

    GtkWidget *radio_color = gtk_check_button_new_with_label("Color Classification");
    gtk_check_button_set_group(GTK_CHECK_BUTTON(radio_color), GTK_CHECK_BUTTON(radio_hist));

    GtkWidget *radio_both = gtk_check_button_new_with_label("Both");
    gtk_check_button_set_group(GTK_CHECK_BUTTON(radio_both), GTK_CHECK_BUTTON(radio_hist));

    gtk_box_append(GTK_BOX(vbox), radio_hist);
    gtk_box_append(GTK_BOX(vbox), radio_color);
    gtk_box_append(GTK_BOX(vbox), radio_both);

    /* Button row */
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_widget_set_halign(hbox, GTK_ALIGN_END);
    GtkWidget *btn_cancel = gtk_button_new_with_label("Cancel");
    GtkWidget *btn_send   = gtk_button_new_with_label("Send");
    gtk_widget_add_css_class(btn_send, "suggested-action");
    gtk_box_append(GTK_BOX(hbox), btn_cancel);
    gtk_box_append(GTK_BOX(hbox), btn_send);
    gtk_box_append(GTK_BOX(vbox), hbox);

    gtk_window_set_child(GTK_WINDOW(dialog_win), vbox);
    gtk_window_present(GTK_WINDOW(dialog_win));

    /* Usar un loop local para simular 'run' y esperar el click */
    GMainLoop *loop = g_main_loop_new(NULL, FALSE);
    int response = GTK_RESPONSE_CANCEL;  /* default */

    /* Callbacks locales */
    g_signal_connect_swapped(btn_cancel, "clicked", G_CALLBACK(g_main_loop_quit), loop);
    g_signal_connect_swapped(btn_send,   "clicked", G_CALLBACK(g_main_loop_quit), loop);

    /* Store pointers to read state after exiting */
    g_object_set_data(G_OBJECT(dialog_win), "radio_hist", radio_hist);
    g_object_set_data(G_OBJECT(dialog_win), "radio_color", radio_color);
    g_object_set_data(G_OBJECT(dialog_win), "radio_both", radio_both);
    g_object_set_data(G_OBJECT(dialog_win), "btn_send", btn_send);

    /* Run the loop: when Send or Cancel is pressed, exit */
    g_main_loop_run(loop);

    /* Decidir respuesta: si el último click fue en 'Send' => OK */
    {
          /* Simple heuristic: if the window still exists and Send has focus,
              or check if Send was the last to emit "clicked".
              A more robust approach is to set a flag in the callbacks. */
        gboolean send_pressed = gtk_widget_has_focus(btn_send);
        response = send_pressed ? GTK_RESPONSE_OK : GTK_RESPONSE_CANCEL;
    }

    /* Leer selección */
    ProcessingType proc_type = PROC_HISTOGRAM;
    if (gtk_check_button_get_active(GTK_CHECK_BUTTON(radio_color))) {
        proc_type = PROC_COLOR_CLASSIFICATION;
    } else if (gtk_check_button_get_active(GTK_CHECK_BUTTON(radio_both))) {
        proc_type = PROC_BOTH;
    }

    g_main_loop_unref(loop);
    gtk_window_destroy(GTK_WINDOW(dialog_win));

    if (response != GTK_RESPONSE_OK) {
        return; /* Cancelado */
    }

    /* --- Ventana de progreso --- */
    GtkWidget *progress_dialog = gtk_window_new();
    gtk_window_set_title(GTK_WINDOW(progress_dialog), "Sending Images");
    gtk_window_set_default_size(GTK_WINDOW(progress_dialog), 400, 150);
    gtk_window_set_transient_for(GTK_WINDOW(progress_dialog), GTK_WINDOW(app_data->window));
    gtk_window_set_modal(GTK_WINDOW(progress_dialog), TRUE);

    GtkWidget *pvbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_margin_start(pvbox, 20);
    gtk_widget_set_margin_end(pvbox, 20);
    gtk_widget_set_margin_top(pvbox, 20);
    gtk_widget_set_margin_bottom(pvbox, 20);

    GtkWidget *label_prog = gtk_label_new("Connecting to server...");
    gtk_box_append(GTK_BOX(pvbox), label_prog);

    GtkWidget *pbar = gtk_progress_bar_new();
    gtk_box_append(GTK_BOX(pvbox), pbar);

    gtk_window_set_child(GTK_WINDOW(progress_dialog), pvbox);
    gtk_window_present(GTK_WINDOW(progress_dialog));

    /* Datos para el hilo */
    SendDialogData dialog_data;
    dialog_data.proc_type = proc_type;
    dialog_data.image_list = app_data->loaded_images;
    dialog_data.label = label_prog;
    dialog_data.progress_bar = pbar;

    /* Lanzar hilo */
    GThread *thread = g_thread_new("send", send_thread_func, &dialog_data);

    /* IMPORTANTE: join correcto (sin gtk_events_pending / gtk_main_iteration) */
    gpointer thread_result = g_thread_join(thread);
    int result = GPOINTER_TO_INT(thread_result);
    (void)result; /* por ahora no lo usamos */

    gtk_window_destroy(GTK_WINDOW(progress_dialog));

    show_message_dialog(GTK_WINDOW(app_data->window),
                        "Transfer Complete",
                        "Image transfer finished!");
}

/*
 * on_credits_button_clicked
 * -------------------------
 * Show the credits dialog when the Credits button is pressed.
 */
void on_credits_button_clicked(GtkWidget *button, gpointer user_data) {
    (void)button; // Suppress unused parameter warning
    AppData *app_data = (AppData *)user_data;
    show_credits_dialog(GTK_WINDOW(app_data->window));
}

/*
 * on_exit_button_clicked
 * ----------------------
 * Clean up loaded images and close the main application window.
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

/*
 * add_image_to_list
 * -----------------
 * Add a visual row for `filepath` into the main window's image
 * list and append the path to `app_data->loaded_images`.
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

/*
 * clear_image_list
 * -----------------
 * Remove all visual entries from the image list widget and free the
 * stored loaded image paths in `app_data`.
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