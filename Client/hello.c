#include <gtk/gtk.h>

static void on_activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *win  = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(win), "Hola GTK");
    gtk_window_set_default_size(GTK_WINDOW(win), 320, 200);
    GtkWidget *btn = gtk_button_new_with_label("Presióname");
    g_signal_connect_swapped(btn, "clicked", G_CALLBACK(gtk_window_close), win);

    gtk_window_set_child(GTK_WINDOW(win), btn);
    gtk_window_present(GTK_WINDOW(win));
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("com.ejemplo.hola", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(on_activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
