#include <gtk/gtk.h>
#include "ftp_client.h"

static GtkWidget *status_text;
static GtkWidget *server_entry;
static GtkWidget *port_entry;
static GtkWidget *user_entry;
static GtkWidget *pass_entry;
static GtkWidget *connect_button;
static GtkWidget *disconnect_button;
static GtkWidget *file_list;
static GtkWidget *current_dir_label;
static GtkWidget *upload_button;
static GtkWidget *download_button;
static GtkWidget *refresh_button;
static GtkWidget *remote_file_entry;
static GtkWidget *local_file_entry;

static ftp_client_t *client = NULL;
static int connected = 0;

static void append_status(const char *message) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(status_text));
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert(buffer, &iter, message, -1);
    gtk_text_buffer_insert(buffer, &iter, "\n", -1);
    
    gtk_text_view_scroll_to_iter(GTK_TEXT_VIEW(status_text), &iter, 0.0, FALSE, 0.0, 0.0);
}

static void update_file_list(void) {
    if (!client || !connected) return;
    
    char list_buffer[BUFFER_SIZE * 4];
    if (ftp_client_list(client, list_buffer, sizeof(list_buffer)) >= 0) {
        GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(file_list)));
        gtk_list_store_clear(store);
        
        char *line = strtok(list_buffer, "\n");
        while (line) {
            // Parse LIST output (simplified - just show filename)
            char *filename = strrchr(line, ' ');
            if (filename) {
                filename++; // Skip space
                char *cr = strchr(filename, '\r');
                if (cr) *cr = '\0';
                
                GtkTreeIter iter;
                gtk_list_store_append(store, &iter);
                gtk_list_store_set(store, &iter, 0, filename, -1);
            }
            line = strtok(NULL, "\n");
        }
    } else {
        append_status("Failed to retrieve file list");
    }
}

static void update_current_dir(void) {
    if (!client || !connected) return;
    
    char pwd[256];
    if (ftp_client_pwd(client, pwd, sizeof(pwd)) == 0) {
        char label_text[512];
        snprintf(label_text, sizeof(label_text), "Current Directory: %s", pwd);
        gtk_label_set_text(GTK_LABEL(current_dir_label), label_text);
    }
}

static void on_connect_clicked(GtkWidget *widget, gpointer data) {
    if (connected) return;
    
    const char *server = gtk_entry_get_text(GTK_ENTRY(server_entry));
    const char *port_str = gtk_entry_get_text(GTK_ENTRY(port_entry));
    const char *username = gtk_entry_get_text(GTK_ENTRY(user_entry));
    const char *password = gtk_entry_get_text(GTK_ENTRY(pass_entry));
    
    if (!server || strlen(server) == 0) {
        append_status("Please enter server address");
        return;
    }
    
    int port = atoi(port_str);
    if (port <= 0) port = FTP_PORT;
    
    client = ftp_client_new();
    if (ftp_client_connect(client, server, port) < 0) {
        append_status("Failed to connect to server");
        ftp_client_free(client);
        client = NULL;
        return;
    }
    
    append_status("Connected to server");
    
    if (username && strlen(username) > 0) {
        if (ftp_client_login(client, username, password ? password : "") == 0) {
            append_status("Login successful");
            connected = 1;
            gtk_widget_set_sensitive(connect_button, FALSE);
            gtk_widget_set_sensitive(disconnect_button, TRUE);
            gtk_widget_set_sensitive(server_entry, FALSE);
            gtk_widget_set_sensitive(port_entry, FALSE);
            gtk_widget_set_sensitive(user_entry, FALSE);
            gtk_widget_set_sensitive(pass_entry, FALSE);
            
            update_current_dir();
            update_file_list();
        } else {
            append_status("Login failed");
            ftp_client_quit(client);
            ftp_client_free(client);
            client = NULL;
        }
    } else {
        append_status("Connected (not logged in)");
        connected = 1;
        gtk_widget_set_sensitive(connect_button, FALSE);
        gtk_widget_set_sensitive(disconnect_button, TRUE);
    }
}

static void on_disconnect_clicked(GtkWidget *widget, gpointer data) {
    if (!connected || !client) return;
    
    ftp_client_quit(client);
    ftp_client_free(client);
    client = NULL;
    connected = 0;
    
    gtk_widget_set_sensitive(connect_button, TRUE);
    gtk_widget_set_sensitive(disconnect_button, FALSE);
    gtk_widget_set_sensitive(server_entry, TRUE);
    gtk_widget_set_sensitive(port_entry, TRUE);
    gtk_widget_set_sensitive(user_entry, TRUE);
    gtk_widget_set_sensitive(pass_entry, TRUE);
    
    GtkListStore *store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(file_list)));
    gtk_list_store_clear(store);
    gtk_label_set_text(GTK_LABEL(current_dir_label), "Current Directory: Not connected");
    append_status("Disconnected from server");
}

static void on_refresh_clicked(GtkWidget *widget, gpointer data) {
    if (!connected || !client) return;
    update_file_list();
    update_current_dir();
    append_status("Directory refreshed");
}

static void on_download_clicked(GtkWidget *widget, gpointer data) {
    if (!connected || !client) return;
    
    const char *remote_file = gtk_entry_get_text(GTK_ENTRY(remote_file_entry));
    const char *local_file = gtk_entry_get_text(GTK_ENTRY(local_file_entry));
    
    if (!remote_file || strlen(remote_file) == 0) {
        append_status("Please enter remote filename");
        return;
    }
    
    if (!local_file || strlen(local_file) == 0) {
        local_file = remote_file;
    }
    
    if (ftp_client_retr(client, remote_file, local_file) == 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Downloaded: %s -> %s", remote_file, local_file);
        append_status(msg);
    } else {
        append_status("Download failed");
    }
}

static void on_upload_clicked(GtkWidget *widget, gpointer data) {
    if (!connected || !client) return;
    
    const char *local_file = gtk_entry_get_text(GTK_ENTRY(local_file_entry));
    const char *remote_file = gtk_entry_get_text(GTK_ENTRY(remote_file_entry));
    
    if (!local_file || strlen(local_file) == 0) {
        append_status("Please enter local filename");
        return;
    }
    
    if (!remote_file || strlen(remote_file) == 0) {
        // Extract filename from path
        const char *filename = strrchr(local_file, '/');
        if (filename) filename++;
        else filename = local_file;
        remote_file = filename;
    }
    
    if (ftp_client_stor(client, local_file, remote_file) == 0) {
        char msg[256];
        snprintf(msg, sizeof(msg), "Uploaded: %s -> %s", local_file, remote_file);
        append_status(msg);
        update_file_list();
    } else {
        append_status("Upload failed");
    }
}

static void on_window_destroy(GtkWidget *widget, gpointer data) {
    if (connected && client) {
        ftp_client_quit(client);
        ftp_client_free(client);
    }
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *vbox, *hbox, *vbox2;
    GtkWidget *label;
    GtkWidget *scrolled_window;
    GtkWidget *frame;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *column;
    GtkListStore *store;
    
    gtk_init(&argc, &argv);
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "FTP Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 900, 750);
    gtk_container_set_border_width(GTK_CONTAINER(window), 10);
    
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    // Connection frame
    frame = gtk_frame_new("Connection");
    vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2), 10);
    gtk_container_add(GTK_CONTAINER(frame), vbox2);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    label = gtk_label_new("Server:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    server_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(server_entry), "127.0.0.1");
    gtk_box_pack_start(GTK_BOX(hbox), server_entry, TRUE, TRUE, 0);
    
    label = gtk_label_new("Port:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    port_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(port_entry), "2121");
    gtk_box_pack_start(GTK_BOX(hbox), port_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    label = gtk_label_new("Username:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    user_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(user_entry), "user");
    gtk_box_pack_start(GTK_BOX(hbox), user_entry, TRUE, TRUE, 0);
    
    label = gtk_label_new("Password:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    pass_entry = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(pass_entry), FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), pass_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    connect_button = gtk_button_new_with_label("Connect");
    g_signal_connect(connect_button, "clicked", G_CALLBACK(on_connect_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), connect_button, FALSE, FALSE, 0);
    
    disconnect_button = gtk_button_new_with_label("Disconnect");
    g_signal_connect(disconnect_button, "clicked", G_CALLBACK(on_disconnect_clicked), NULL);
    gtk_widget_set_sensitive(disconnect_button, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), disconnect_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    // Current directory
    current_dir_label = gtk_label_new("Current Directory: Not connected");
    gtk_box_pack_start(GTK_BOX(vbox), current_dir_label, FALSE, FALSE, 0);
    
    // File list
    frame = gtk_frame_new("Remote Files");
    vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2), 10);
    gtk_container_add(GTK_CONTAINER(frame), vbox2);
    
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled_window), 300);
    gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(scrolled_window), 400);
    
    store = gtk_list_store_new(1, G_TYPE_STRING);
    file_list = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    renderer = gtk_cell_renderer_text_new();
    column = gtk_tree_view_column_new_with_attributes("Filename", renderer, "text", 0, NULL);
    gtk_tree_view_column_set_min_width(column, 350);
    gtk_tree_view_append_column(GTK_TREE_VIEW(file_list), column);
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), file_list);
    gtk_box_pack_start(GTK_BOX(vbox2), scrolled_window, TRUE, TRUE, 0);
    
    refresh_button = gtk_button_new_with_label("Refresh");
    g_signal_connect(refresh_button, "clicked", G_CALLBACK(on_refresh_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox2), refresh_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
    
    // File transfer
    frame = gtk_frame_new("File Transfer");
    vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2), 10);
    gtk_container_add(GTK_CONTAINER(frame), vbox2);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    label = gtk_label_new("Local File:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    local_file_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), local_file_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    label = gtk_label_new("Remote File:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    remote_file_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), remote_file_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    upload_button = gtk_button_new_with_label("Upload");
    g_signal_connect(upload_button, "clicked", G_CALLBACK(on_upload_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), upload_button, FALSE, FALSE, 0);
    
    download_button = gtk_button_new_with_label("Download");
    g_signal_connect(download_button, "clicked", G_CALLBACK(on_download_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), download_button, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), hbox, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    // Status area
    frame = gtk_frame_new("Status");
    vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(vbox2), 10);
    gtk_container_add(GTK_CONTAINER(frame), vbox2);
    
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(scrolled_window), 150);
    
    status_text = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(status_text), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(status_text), TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), status_text);
    gtk_box_pack_start(GTK_BOX(vbox2), scrolled_window, TRUE, TRUE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), frame, TRUE, TRUE, 0);
    
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    
    gtk_widget_show_all(window);
    
    append_status("FTP Client Ready");
    append_status("Enter server details and click 'Connect'");
    
    gtk_main();
    
    return 0;
}

