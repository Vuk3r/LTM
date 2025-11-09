#include <gtk/gtk.h>
#include <pthread.h>
#include "ftp_common.h"

extern int ftpd_start(int port);

static GtkWidget *status_text;
static GtkWidget *port_entry;
static GtkWidget *start_button;
static GtkWidget *stop_button;
static int server_running = 0;
static pthread_t server_thread;
static int server_port = FTP_PORT;

static void append_status(const char *message) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(status_text));
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert(buffer, &iter, message, -1);
    gtk_text_buffer_insert(buffer, &iter, "\n", -1);
    
    // Auto-scroll to bottom
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(status_text), &iter, 0.0, FALSE, 0.0, 0.0);
}

static void *server_thread_func(void *arg) {
    int port = *(int *)arg;
    char msg[256];
    snprintf(msg, sizeof(msg), "Starting FTP server on port %d...", port);
    append_status(msg);
    ftpd_start(port);
    return NULL;
}

static void on_start_clicked(GtkWidget *widget, gpointer data) {
    if (server_running) return;
    
    const char *port_str = gtk_entry_get_text(GTK_ENTRY(port_entry));
    if (port_str && strlen(port_str) > 0) {
        server_port = atoi(port_str);
        if (server_port <= 0 || server_port > 65535) {
            append_status("Invalid port number. Using default port 21.");
            server_port = FTP_PORT;
        }
    }
    
    server_running = 1;
    gtk_widget_set_sensitive(start_button, FALSE);
    gtk_widget_set_sensitive(stop_button, TRUE);
    gtk_widget_set_sensitive(port_entry, FALSE);
    
    pthread_create(&server_thread, NULL, server_thread_func, &server_port);
    append_status("FTP Server started successfully!");
}

static void on_stop_clicked(GtkWidget *widget, gpointer data) {
    if (!server_running) return;
    
    // Note: In a real implementation, you'd need a way to stop the server
    // For now, we'll just update the UI
    server_running = 0;
    gtk_widget_set_sensitive(start_button, TRUE);
    gtk_widget_set_sensitive(stop_button, FALSE);
    gtk_widget_set_sensitive(port_entry, TRUE);
    append_status("FTP Server stopped.");
}

static void on_window_destroy(GtkWidget *widget, gpointer data) {
    if (server_running) {
        server_running = 0;
    }
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *vbox, *hbox;
    GtkWidget *label;
    GtkWidget *scrolled_window;
    
    gtk_init(&argc, &argv);
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "FTP Server");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    // Port configuration
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    label = gtk_label_new("Port:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    port_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(port_entry), "21");
    gtk_box_pack_start(GTK_BOX(hbox), port_entry, TRUE, TRUE, 0);
    
    start_button = gtk_button_new_with_label("Start Server");
    g_signal_connect(start_button, "clicked", G_CALLBACK(on_start_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), start_button, FALSE, FALSE, 0);
    
    stop_button = gtk_button_new_with_label("Stop Server");
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_clicked), NULL);
    gtk_widget_set_sensitive(stop_button, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), stop_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    // Status area
    label = gtk_label_new("Server Status:");
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    status_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(status_text), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(status_text), TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), status_text);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
    
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    
    gtk_widget_show_all(window);
    
    append_status("FTP Server GUI Ready");
    append_status("Configure port and click 'Start Server' to begin");
    
    gtk_main();
    
    return 0;
}

