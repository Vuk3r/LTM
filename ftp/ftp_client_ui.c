#include <gtk/gtk.h>
#include "ftp_client.h"

static GtkWidget *server_ip_entry;
static GtkWidget *server_port_entry;
static GtkWidget *username_entry;
static GtkWidget *password_entry;
static GtkWidget *connect_button;
static GtkWidget *disconnect_button;
static GtkWidget *file_list_text;
static GtkTextBuffer *file_list_buffer;
static GtkWidget *remote_file_entry;
static GtkWidget *local_file_entry;
static GtkWidget *upload_button;
static GtkWidget *download_button;
static GtkWidget *refresh_button;
static GtkWidget *status_label;

static ftp_client_t client;
static gboolean connected = FALSE;

// Forward declarations
static void on_refresh_clicked(GtkWidget *widget, gpointer data);

static void update_status(const char *message) {
    gtk_label_set_text(GTK_LABEL(status_label), message);
}

static void on_connect_clicked(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data; // Suppress unused parameter warnings
    if (connected) return;
    
    const char *ip = gtk_entry_get_text(GTK_ENTRY(server_ip_entry));
    const char *port_str = gtk_entry_get_text(GTK_ENTRY(server_port_entry));
    const char *username = gtk_entry_get_text(GTK_ENTRY(username_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(password_entry));
    
    int port = atoi(port_str);
    if (port <= 0 || port > 65535) {
        update_status("Invalid port number");
        return;
    }
    
    if (ftp_connect(&client, ip, port) < 0) {
        update_status("Connection failed");
        return;
    }
    
    if (ftp_login(&client, username, password) < 0) {
        update_status("Login failed");
        ftp_disconnect(&client);
        return;
    }
    
    connected = TRUE;
    gtk_widget_set_sensitive(connect_button, FALSE);
    gtk_widget_set_sensitive(disconnect_button, TRUE);
    gtk_widget_set_sensitive(server_ip_entry, FALSE);
    gtk_widget_set_sensitive(server_port_entry, FALSE);
    gtk_widget_set_sensitive(username_entry, FALSE);
    gtk_widget_set_sensitive(password_entry, FALSE);
    gtk_widget_set_sensitive(refresh_button, TRUE);
    gtk_widget_set_sensitive(upload_button, TRUE);
    gtk_widget_set_sensitive(download_button, TRUE);
    
    update_status("Connected");
    
    // Refresh file list
    on_refresh_clicked(NULL, NULL);
}

static void on_disconnect_clicked(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data; // Suppress unused parameter warnings
    if (!connected) return;
    
    ftp_disconnect(&client);
    connected = FALSE;
    
    gtk_widget_set_sensitive(connect_button, TRUE);
    gtk_widget_set_sensitive(disconnect_button, FALSE);
    gtk_widget_set_sensitive(server_ip_entry, TRUE);
    gtk_widget_set_sensitive(server_port_entry, TRUE);
    gtk_widget_set_sensitive(username_entry, TRUE);
    gtk_widget_set_sensitive(password_entry, TRUE);
    gtk_widget_set_sensitive(refresh_button, FALSE);
    gtk_widget_set_sensitive(upload_button, FALSE);
    gtk_widget_set_sensitive(download_button, FALSE);
    
    gtk_text_buffer_set_text(file_list_buffer, "", -1);
    update_status("Disconnected");
}

static void on_refresh_clicked(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data; // Suppress unused parameter warnings
    if (!connected) return;
    
    char buffer[FTP_BUFFER_SIZE];
    if (ftp_list(&client, buffer, sizeof(buffer)) == 0) {
        gtk_text_buffer_set_text(file_list_buffer, buffer, -1);
        update_status("File list refreshed");
    } else {
        update_status("Failed to refresh file list");
    }
}

static void on_upload_clicked(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data; // Suppress unused parameter warnings
    if (!connected) return;
    
    const char *local_file = gtk_entry_get_text(GTK_ENTRY(local_file_entry));
    const char *remote_file = gtk_entry_get_text(GTK_ENTRY(remote_file_entry));
    
    if (strlen(local_file) == 0 || strlen(remote_file) == 0) {
        update_status("Please specify both local and remote file names");
        return;
    }
    
    if (ftp_stor(&client, local_file, remote_file) == 0) {
        update_status("File uploaded successfully");
        on_refresh_clicked(NULL, NULL);
    } else {
        update_status("Upload failed");
    }
}

static void on_download_clicked(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data; // Suppress unused parameter warnings
    if (!connected) return;
    
    const char *remote_file = gtk_entry_get_text(GTK_ENTRY(remote_file_entry));
    const char *local_file = gtk_entry_get_text(GTK_ENTRY(local_file_entry));
    
    if (strlen(remote_file) == 0 || strlen(local_file) == 0) {
        update_status("Please specify both remote and local file names");
        return;
    }
    
    if (ftp_retr(&client, remote_file, local_file) == 0) {
        update_status("File downloaded successfully");
    } else {
        update_status("Download failed");
    }
}

static void on_destroy(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data; // Suppress unused parameter warnings
    if (connected) {
        ftp_disconnect(&client);
    }
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);
    
    memset(&client, 0, sizeof(client));
    
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "FTP Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(on_destroy), NULL);
    
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    
    // Connection settings
    GtkWidget *conn_frame = gtk_frame_new("Connection Settings");
    GtkWidget *conn_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(conn_frame), conn_vbox);
    gtk_container_set_border_width(GTK_CONTAINER(conn_vbox), 5);
    
    GtkWidget *ip_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *ip_label = gtk_label_new("Server IP:");
    gtk_widget_set_size_request(ip_label, 100, -1);
    server_ip_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(server_ip_entry), "127.0.0.1");
    gtk_box_pack_start(GTK_BOX(ip_box), ip_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(ip_box), server_ip_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(conn_vbox), ip_box, FALSE, FALSE, 0);
    
    GtkWidget *port_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *port_label = gtk_label_new("Port:");
    gtk_widget_set_size_request(port_label, 100, -1);
    server_port_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(server_port_entry), "21");
    gtk_box_pack_start(GTK_BOX(port_box), port_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(port_box), server_port_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(conn_vbox), port_box, FALSE, FALSE, 0);
    
    GtkWidget *user_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *user_label = gtk_label_new("Username:");
    gtk_widget_set_size_request(user_label, 100, -1);
    username_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(username_entry), "user");
    gtk_box_pack_start(GTK_BOX(user_box), user_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(user_box), username_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(conn_vbox), user_box, FALSE, FALSE, 0);
    
    GtkWidget *pass_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *pass_label = gtk_label_new("Password:");
    gtk_widget_set_size_request(pass_label, 100, -1);
    password_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(password_entry), FALSE);
    gtk_entry_set_text(GTK_ENTRY(password_entry), "pass");
    gtk_box_pack_start(GTK_BOX(pass_box), pass_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pass_box), password_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(conn_vbox), pass_box, FALSE, FALSE, 0);
    
    GtkWidget *conn_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    connect_button = gtk_button_new_with_label("Connect");
    disconnect_button = gtk_button_new_with_label("Disconnect");
    gtk_widget_set_sensitive(disconnect_button, FALSE);
    g_signal_connect(connect_button, "clicked", G_CALLBACK(on_connect_clicked), NULL);
    g_signal_connect(disconnect_button, "clicked", G_CALLBACK(on_disconnect_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(conn_button_box), connect_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(conn_button_box), disconnect_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(conn_vbox), conn_button_box, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), conn_frame, FALSE, FALSE, 0);
    
    // File list
    GtkWidget *list_frame = gtk_frame_new("Remote Files");
    GtkWidget *list_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(list_frame), list_vbox);
    gtk_container_set_border_width(GTK_CONTAINER(list_vbox), 5);
    
    GtkWidget *list_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    refresh_button = gtk_button_new_with_label("Refresh");
    gtk_widget_set_sensitive(refresh_button, FALSE);
    g_signal_connect(refresh_button, "clicked", G_CALLBACK(on_refresh_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(list_button_box), refresh_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(list_vbox), list_button_box, FALSE, FALSE, 0);
    
    GtkWidget *scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    file_list_text = gtk_text_view_new();
    file_list_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(file_list_text));
    gtk_text_view_set_editable(GTK_TEXT_VIEW(file_list_text), FALSE);
    gtk_container_add(GTK_CONTAINER(scrolled), file_list_text);
    gtk_box_pack_start(GTK_BOX(list_vbox), scrolled, TRUE, TRUE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), list_frame, TRUE, TRUE, 0);
    
    // File transfer
    GtkWidget *transfer_frame = gtk_frame_new("File Transfer");
    GtkWidget *transfer_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(transfer_frame), transfer_vbox);
    gtk_container_set_border_width(GTK_CONTAINER(transfer_vbox), 5);
    
    GtkWidget *remote_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *remote_label = gtk_label_new("Remote File:");
    gtk_widget_set_size_request(remote_label, 100, -1);
    remote_file_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(remote_box), remote_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(remote_box), remote_file_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(transfer_vbox), remote_box, FALSE, FALSE, 0);
    
    GtkWidget *local_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    GtkWidget *local_label = gtk_label_new("Local File:");
    gtk_widget_set_size_request(local_label, 100, -1);
    local_file_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(local_box), local_label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(local_box), local_file_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(transfer_vbox), local_box, FALSE, FALSE, 0);
    
    GtkWidget *transfer_button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    upload_button = gtk_button_new_with_label("Upload");
    download_button = gtk_button_new_with_label("Download");
    gtk_widget_set_sensitive(upload_button, FALSE);
    gtk_widget_set_sensitive(download_button, FALSE);
    g_signal_connect(upload_button, "clicked", G_CALLBACK(on_upload_clicked), NULL);
    g_signal_connect(download_button, "clicked", G_CALLBACK(on_download_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(transfer_button_box), upload_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(transfer_button_box), download_button, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(transfer_vbox), transfer_button_box, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), transfer_frame, FALSE, FALSE, 0);
    
    // Status
    status_label = gtk_label_new("Not connected");
    gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, FALSE, 0);
    
    gtk_widget_show_all(window);
    gtk_main();
    
    return 0;
}