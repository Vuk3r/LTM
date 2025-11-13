#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#define DEFAULT_PORT 9001
#define BUF_SIZE 2048

// Global variables
int sockfd = -1;
GtkWidget *chat_text_view;
GtkWidget *message_entry;
GtkWidget *server_ip_entry;
GtkWidget *server_port_entry;
GtkWidget *connect_button;
GtkWidget *disconnect_button;
GtkWidget *send_button;
GtkWidget *broadcast_radio;
GtkWidget *unicast_radio;
GtkWidget *multicast_radio;
GtkWidget *recipient_entry;
GtkTextBuffer *chat_buffer;
int client_id = 0;
int connected = 0;
pthread_t recv_thread = 0;
int recv_thread_active = 0;

// Message types
typedef enum {
    MSG_BROADCAST,
    MSG_UNICAST,
    MSG_MULTICAST
} msg_type_t;

// Add message to chat display (for main thread)
void append_to_chat(const char *text) {
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(chat_buffer, &iter);
    gtk_text_buffer_insert(chat_buffer, &iter, text, -1);
    
    // Auto-scroll to bottom
    gtk_text_buffer_get_end_iter(chat_buffer, &iter);
    GtkTextMark *mark = gtk_text_buffer_create_mark(chat_buffer, "end", &iter, FALSE);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(chat_text_view), mark, 0.0, FALSE, 0.0, 0.0);
    gtk_text_buffer_delete_mark(chat_buffer, mark);
}

// Add message to chat display (thread-safe callback)
gboolean append_to_chat_cb(gpointer data) {
    char *text = (char *)data;
    GtkTextIter iter;
    gtk_text_buffer_get_end_iter(chat_buffer, &iter);
    gtk_text_buffer_insert(chat_buffer, &iter, text, -1);
    
    // Auto-scroll to bottom
    gtk_text_buffer_get_end_iter(chat_buffer, &iter);
    GtkTextMark *mark = gtk_text_buffer_create_mark(chat_buffer, "end", &iter, FALSE);
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(chat_text_view), mark, 0.0, FALSE, 0.0, 0.0);
    gtk_text_buffer_delete_mark(chat_buffer, mark);
    
    g_free(text);
    return FALSE;
}

// Set widget sensitive (thread-safe callback)
gboolean set_widget_sensitive_cb(gpointer data) {
    GtkWidget *widget = (GtkWidget *)data;
    gboolean sensitive = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget), "sensitive"));
    gtk_widget_set_sensitive(widget, sensitive);
    return FALSE;
}

// Receive messages thread
void *receive_messages(void *arg) {
    (void)arg; // Unused parameter
    char buffer[BUF_SIZE];
    int bytes_read;
    
    while (connected && (bytes_read = recv(sockfd, buffer, BUF_SIZE - 1, 0)) > 0) {
        buffer[bytes_read] = '\0';
        
        // Update UI from main thread
        g_idle_add(append_to_chat_cb, g_strdup(buffer));
    }
    
    if (connected) {
        g_idle_add(append_to_chat_cb, g_strdup("\n[Disconnected from server]\n"));
        connected = 0;
        recv_thread_active = 0;
        
        // Update UI widgets from main thread
        g_object_set_data(G_OBJECT(connect_button), "sensitive", GINT_TO_POINTER(TRUE));
        g_idle_add(set_widget_sensitive_cb, connect_button);
        
        g_object_set_data(G_OBJECT(disconnect_button), "sensitive", GINT_TO_POINTER(FALSE));
        g_idle_add(set_widget_sensitive_cb, disconnect_button);
        
        g_object_set_data(G_OBJECT(send_button), "sensitive", GINT_TO_POINTER(FALSE));
        g_idle_add(set_widget_sensitive_cb, send_button);
        
        g_object_set_data(G_OBJECT(server_ip_entry), "sensitive", GINT_TO_POINTER(TRUE));
        g_idle_add(set_widget_sensitive_cb, server_ip_entry);
        
        g_object_set_data(G_OBJECT(server_port_entry), "sensitive", GINT_TO_POINTER(TRUE));
        g_idle_add(set_widget_sensitive_cb, server_port_entry);
    }
    
    return NULL;
}

// Send message to server
void send_message_to_server(const char *message, msg_type_t type, const char *recipients) {
    if (!connected || sockfd < 0) return;
    
    char formatted_msg[BUF_SIZE];
    const char *type_str;
    
    switch (type) {
        case MSG_BROADCAST:
            type_str = "BROADCAST";
            snprintf(formatted_msg, sizeof(formatted_msg), "%s::%s\n", type_str, message);
            break;
        case MSG_UNICAST:
            type_str = "UNICAST";
            snprintf(formatted_msg, sizeof(formatted_msg), "%s:%s:%s\n", type_str, recipients, message);
            break;
        case MSG_MULTICAST:
            type_str = "MULTICAST";
            snprintf(formatted_msg, sizeof(formatted_msg), "%s:%s:%s\n", type_str, recipients, message);
            break;
    }
    
    send(sockfd, formatted_msg, strlen(formatted_msg), 0);
}

// Connect to server
void on_connect_clicked(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data; // Unused parameters
    const char *server_ip_str = gtk_entry_get_text(GTK_ENTRY(server_ip_entry));
    const char *server_port_str = gtk_entry_get_text(GTK_ENTRY(server_port_entry));
    
    if (strlen(server_ip_str) == 0) {
        append_to_chat("[Error] Please enter server IP address\n");
        return;
    }
    
    if (strlen(server_port_str) == 0) {
        append_to_chat("[Error] Please enter server port\n");
        return;
    }
    
    int port = atoi(server_port_str);
    if (port <= 0 || port > 65535) {
        append_to_chat("[Error] Invalid port number. Port must be between 1 and 65535.\n");
        return;
    }
    
    struct sockaddr_in server_addr;
    
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        append_to_chat("[Error] Socket creation failed\n");
        return;
    }
    
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip_str, &server_addr.sin_addr) <= 0) {
        append_to_chat("[Error] Invalid IP address\n");
        close(sockfd);
        sockfd = -1;
        return;
    }
    
    // Set socket options for better connection handling
    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        char error_msg[BUF_SIZE];
        snprintf(error_msg, sizeof(error_msg), "[Error] Connection failed: %s\n", strerror(errno));
        append_to_chat(error_msg);
        append_to_chat("[Hint] Make sure the server is running and the IP/port are correct.\n");
        append_to_chat("[Hint] For LAN connections, use the server's LAN IP (e.g., 192.168.1.x), not 127.0.0.1\n");
        close(sockfd);
        sockfd = -1;
        return;
    }
    
    connected = 1;
    gtk_widget_set_sensitive(connect_button, FALSE);
    gtk_widget_set_sensitive(disconnect_button, TRUE);
    gtk_widget_set_sensitive(server_ip_entry, FALSE);
    gtk_widget_set_sensitive(server_port_entry, FALSE);
    gtk_widget_set_sensitive(send_button, TRUE);
    
    char conn_msg[BUF_SIZE];
    snprintf(conn_msg, sizeof(conn_msg), "[Connected to server %s:%d]\n", server_ip_str, port);
    append_to_chat(conn_msg);
    
    // Start receive thread
    recv_thread_active = 1;
    if (pthread_create(&recv_thread, NULL, receive_messages, NULL) != 0) {
        append_to_chat("[Error] Failed to create receive thread\n");
        recv_thread_active = 0;
        connected = 0;
        close(sockfd);
        sockfd = -1;
        gtk_widget_set_sensitive(connect_button, TRUE);
        gtk_widget_set_sensitive(disconnect_button, FALSE);
        gtk_widget_set_sensitive(server_ip_entry, TRUE);
        gtk_widget_set_sensitive(server_port_entry, TRUE);
        gtk_widget_set_sensitive(send_button, FALSE);
        return;
    }
}

// Disconnect from server
void on_disconnect_clicked(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data; // Unused parameters
    connected = 0;
    if (sockfd >= 0) {
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        sockfd = -1;
    }
    
    // Wait for receive thread to finish
    if (recv_thread_active) {
        pthread_join(recv_thread, NULL);
        recv_thread_active = 0;
    }
    
    gtk_widget_set_sensitive(connect_button, TRUE);
    gtk_widget_set_sensitive(disconnect_button, FALSE);
    gtk_widget_set_sensitive(server_ip_entry, TRUE);
    gtk_widget_set_sensitive(server_port_entry, TRUE);
    gtk_widget_set_sensitive(send_button, FALSE);
    
    append_to_chat("[Disconnected from server]\n");
}

// Send message button clicked
void on_send_clicked(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data; // Unused parameters
    const char *message = gtk_entry_get_text(GTK_ENTRY(message_entry));
    
    if (strlen(message) == 0) return;
    
    msg_type_t type;
    const char *recipients = "";
    
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(broadcast_radio))) {
        type = MSG_BROADCAST;
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(unicast_radio))) {
        type = MSG_UNICAST;
        recipients = gtk_entry_get_text(GTK_ENTRY(recipient_entry));
        if (strlen(recipients) == 0) {
            append_to_chat("[Error] Please enter recipient ID for unicast\n");
            return;
        }
    } else if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(multicast_radio))) {
        type = MSG_MULTICAST;
        recipients = gtk_entry_get_text(GTK_ENTRY(recipient_entry));
        if (strlen(recipients) == 0) {
            append_to_chat("[Error] Please enter recipient IDs (comma-separated) for multicast\n");
            return;
        }
    } else {
        type = MSG_BROADCAST; // Default
    }
    
    send_message_to_server(message, type, recipients);
    
    // Display sent message in chat
    char display_msg[BUF_SIZE];
    const char *type_str = (type == MSG_BROADCAST) ? "Broadcast" : 
                          (type == MSG_UNICAST) ? "Unicast" : "Multicast";
    if (type == MSG_BROADCAST) {
        snprintf(display_msg, sizeof(display_msg), "[You - %s]: %s\n", type_str, message);
    } else {
        snprintf(display_msg, sizeof(display_msg), "[You - %s to %s]: %s\n", type_str, recipients, message);
    }
    append_to_chat(display_msg);
    
    // Clear message entry
    gtk_entry_set_text(GTK_ENTRY(message_entry), "");
}

// Handle Enter key in message entry
gboolean on_message_entry_activate(GtkWidget *widget, gpointer data) {
    if (connected) {
        on_send_clicked(widget, data);
    }
    return TRUE;
}

// Update recipient entry sensitivity
void on_message_type_changed(GtkWidget *widget, gpointer data) {
    (void)widget; (void)data; // Unused parameters
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(broadcast_radio))) {
        gtk_widget_set_sensitive(recipient_entry, FALSE);
    } else {
        gtk_widget_set_sensitive(recipient_entry, TRUE);
    }
}

// Window close handler
gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
    (void)widget; (void)event; (void)data; // Unused parameters
    if (connected) {
        connected = 0;
        if (sockfd >= 0) {
            shutdown(sockfd, SHUT_RDWR);
            close(sockfd);
            sockfd = -1;
        }
        // Wait for receive thread to finish
        if (recv_thread_active) {
            pthread_join(recv_thread, NULL);
            recv_thread_active = 0;
        }
    }
    gtk_main_quit();
    return FALSE;
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *vbox, *hbox, *hbox2, *hbox3;
    GtkWidget *label;
    GtkWidget *scrolled_window;
    GtkWidget *frame;
    
    gtk_init(&argc, &argv);
    
    // Create main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Chat Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(window, "delete-event", G_CALLBACK(on_window_delete), NULL);
    
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    
    // Connection section
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    label = gtk_label_new("Server IP:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    server_ip_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(server_ip_entry), "Enter server IP (e.g., 192.168.1.100)");
    gtk_box_pack_start(GTK_BOX(hbox), server_ip_entry, TRUE, TRUE, 0);
    
    label = gtk_label_new("Port:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    
    server_port_entry = gtk_entry_new();
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", DEFAULT_PORT);
    gtk_entry_set_text(GTK_ENTRY(server_port_entry), port_str);
    gtk_entry_set_max_length(GTK_ENTRY(server_port_entry), 5);
    gtk_box_pack_start(GTK_BOX(hbox), server_port_entry, FALSE, FALSE, 0);
    
    connect_button = gtk_button_new_with_label("Connect");
    g_signal_connect(connect_button, "clicked", G_CALLBACK(on_connect_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), connect_button, FALSE, FALSE, 0);
    
    disconnect_button = gtk_button_new_with_label("Disconnect");
    gtk_widget_set_sensitive(disconnect_button, FALSE);
    g_signal_connect(disconnect_button, "clicked", G_CALLBACK(on_disconnect_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), disconnect_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
    
    // Message type selection
    frame = gtk_frame_new("Message Type");
    hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(hbox2), 5);
    
    broadcast_radio = gtk_radio_button_new_with_label(NULL, "Broadcast");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(broadcast_radio), TRUE);
    g_signal_connect(broadcast_radio, "toggled", G_CALLBACK(on_message_type_changed), NULL);
    gtk_box_pack_start(GTK_BOX(hbox2), broadcast_radio, FALSE, FALSE, 0);
    
    unicast_radio = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(broadcast_radio), "Unicast");
    g_signal_connect(unicast_radio, "toggled", G_CALLBACK(on_message_type_changed), NULL);
    gtk_box_pack_start(GTK_BOX(hbox2), unicast_radio, FALSE, FALSE, 0);
    
    multicast_radio = gtk_radio_button_new_with_label_from_widget(
        GTK_RADIO_BUTTON(broadcast_radio), "Multicast");
    g_signal_connect(multicast_radio, "toggled", G_CALLBACK(on_message_type_changed), NULL);
    gtk_box_pack_start(GTK_BOX(hbox2), multicast_radio, FALSE, FALSE, 0);
    
    label = gtk_label_new("Recipient(s):");
    gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
    
    recipient_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(recipient_entry), "ID or IDs (comma-separated)");
    gtk_widget_set_sensitive(recipient_entry, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox2), recipient_entry, TRUE, TRUE, 0);
    
    gtk_container_add(GTK_CONTAINER(frame), hbox2);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    // Chat display
    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    
    chat_text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(chat_text_view), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(chat_text_view), GTK_WRAP_WORD);
    chat_buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(chat_text_view));
    
    gtk_container_add(GTK_CONTAINER(scrolled_window), chat_text_view);
    gtk_box_pack_start(GTK_BOX(vbox), scrolled_window, TRUE, TRUE, 0);
    
    // Message input section
    hbox3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    label = gtk_label_new("Message:");
    gtk_box_pack_start(GTK_BOX(hbox3), label, FALSE, FALSE, 0);
    
    message_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(message_entry), "Type your message here...");
    g_signal_connect(message_entry, "activate", G_CALLBACK(on_message_entry_activate), NULL);
    gtk_box_pack_start(GTK_BOX(hbox3), message_entry, TRUE, TRUE, 0);
    
    send_button = gtk_button_new_with_label("Send");
    gtk_widget_set_sensitive(send_button, FALSE);
    g_signal_connect(send_button, "clicked", G_CALLBACK(on_send_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox3), send_button, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(vbox), hbox3, FALSE, FALSE, 0);
    
    // Show all widgets
    gtk_widget_show_all(window);
    
    append_to_chat("Welcome to Chat Client!\n");
    append_to_chat("Enter server IP address (e.g., 192.168.1.100) and port, then click Connect.\n");
    append_to_chat("For LAN connections, use the server's LAN IP address, not 127.0.0.1\n\n");
    
    gtk_main();
    
    return 0;
}
