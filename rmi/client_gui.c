#include "rmi.h"
#include <gtk/gtk.h>

static int sock = -1;
static GtkWidget *ip_entry;
static GtkWidget *port_entry;
static GtkWidget *connect_btn;
static GtkWidget *disconnect_btn;
static GtkWidget *method_combo;
static GtkWidget *arg1_entry;
static GtkWidget *arg2_entry;
static GtkWidget *result_label;
static GtkWidget *status_label;
static GtkWidget *calculate_btn;

double invoke_remote_method(int sock, MethodID method_id, double arg1, double arg2) {
    RMIRequest req;
    RMIResponse resp;
    
    if (sock < 0) {
        return -999999.0;
    }
    
    req.method_id = method_id;
    req.arg1 = arg1;
    req.arg2 = arg2;
    
    int sent = send(sock, &req, sizeof(RMIRequest), 0);
    if (sent != sizeof(RMIRequest)) {
        return -999999.0;
    }
    
    int valread = recv(sock, &resp, sizeof(RMIResponse), 0);
    if (valread != sizeof(RMIResponse)) {
        return -999999.0;
    }
    
    if (!resp.success) {
        return -999999.0;
    }
    
    return resp.result;
}

void on_connect_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    const char *ip = gtk_entry_get_text(GTK_ENTRY(ip_entry));
    const char *port_str = gtk_entry_get_text(GTK_ENTRY(port_entry));
    int port = atoi(port_str);
    
    if (port <= 0 || port > 65535) {
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Invalid port number");
        return;
    }
    
    struct sockaddr_in serv_addr;
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Invalid IP address");
        return;
    }
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Socket creation failed");
        return;
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Connection failed");
        close(sock);
        sock = -1;
        return;
    }
    
    gtk_label_set_text(GTK_LABEL(status_label), "Status: Connected");
    gtk_widget_set_sensitive(connect_btn, FALSE);
    gtk_widget_set_sensitive(disconnect_btn, TRUE);
    gtk_widget_set_sensitive(calculate_btn, TRUE);
    gtk_widget_set_sensitive(ip_entry, FALSE);
    gtk_widget_set_sensitive(port_entry, FALSE);
}

void on_disconnect_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    if (sock >= 0) {
        invoke_remote_method(sock, METHOD_EXIT, 0, 0);
        close(sock);
        sock = -1;
    }
    
    gtk_label_set_text(GTK_LABEL(status_label), "Status: Disconnected");
    gtk_widget_set_sensitive(connect_btn, TRUE);
    gtk_widget_set_sensitive(disconnect_btn, FALSE);
    gtk_widget_set_sensitive(calculate_btn, FALSE);
    gtk_widget_set_sensitive(ip_entry, TRUE);
    gtk_widget_set_sensitive(port_entry, TRUE);
    gtk_label_set_text(GTK_LABEL(result_label), "Result: --");
}

void on_calculate_clicked(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    if (sock < 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Not connected");
        return;
    }
    
    const char *arg1_str = gtk_entry_get_text(GTK_ENTRY(arg1_entry));
    const char *arg2_str = gtk_entry_get_text(GTK_ENTRY(arg2_entry));
    
    if (strlen(arg1_str) == 0 || strlen(arg2_str) == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Please enter both arguments");
        return;
    }
    
    double arg1 = atof(arg1_str);
    double arg2 = atof(arg2_str);
    
    int method_index = gtk_combo_box_get_active(GTK_COMBO_BOX(method_combo));
    MethodID method_id = method_index + 1;
    
    if (method_id < METHOD_ADD || method_id > METHOD_DIVIDE) {
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Please select a method");
        return;
    }
    
    double result = invoke_remote_method(sock, method_id, arg1, arg2);
    
    if (result == -999999.0) {
        gtk_label_set_text(GTK_LABEL(result_label), "Result: Error");
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Connection error - please reconnect");
        sock = -1;
        gtk_widget_set_sensitive(connect_btn, TRUE);
        gtk_widget_set_sensitive(disconnect_btn, FALSE);
        gtk_widget_set_sensitive(calculate_btn, FALSE);
        gtk_widget_set_sensitive(ip_entry, TRUE);
        gtk_widget_set_sensitive(port_entry, TRUE);
    } else {
        char result_str[64];
        snprintf(result_str, sizeof(result_str), "Result: %.2f", result);
        gtk_label_set_text(GTK_LABEL(result_label), result_str);
        gtk_label_set_text(GTK_LABEL(status_label), "Status: Calculation successful");
    }
}

void on_window_destroy(GtkWidget *widget, gpointer data) {
    (void)widget;
    (void)data;
    if (sock >= 0) {
        invoke_remote_method(sock, METHOD_EXIT, 0, 0);
        close(sock);
    }
    gtk_main_quit();
}

int main(int argc, char *argv[]) {
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *hbox;
    GtkWidget *label;
    GtkWidget *frame;
    
    gtk_init(&argc, &argv);
    
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "RMI Client");
    gtk_window_set_default_size(GTK_WINDOW(window), 400, 500);
    gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_destroy), NULL);
    
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 20);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    
    frame = gtk_frame_new("Connection");
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    GtkWidget *conn_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(conn_vbox), 10);
    gtk_container_add(GTK_CONTAINER(frame), conn_vbox);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    label = gtk_label_new("Server IP:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    ip_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(ip_entry), "192.168.1.79");
    gtk_box_pack_start(GTK_BOX(hbox), ip_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(conn_vbox), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    label = gtk_label_new("Port:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    port_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(port_entry), "8080");
    gtk_box_pack_start(GTK_BOX(hbox), port_entry, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(conn_vbox), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    connect_btn = gtk_button_new_with_label("Connect");
    g_signal_connect(connect_btn, "clicked", G_CALLBACK(on_connect_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox), connect_btn, TRUE, TRUE, 0);
    
    disconnect_btn = gtk_button_new_with_label("Disconnect");
    g_signal_connect(disconnect_btn, "clicked", G_CALLBACK(on_disconnect_clicked), NULL);
    gtk_widget_set_sensitive(disconnect_btn, FALSE);
    gtk_box_pack_start(GTK_BOX(hbox), disconnect_btn, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(conn_vbox), hbox, FALSE, FALSE, 0);
    
    frame = gtk_frame_new("Remote Method");
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);
    
    GtkWidget *method_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_set_border_width(GTK_CONTAINER(method_vbox), 10);
    gtk_container_add(GTK_CONTAINER(frame), method_vbox);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    label = gtk_label_new("Method:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    method_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(method_combo), "Add");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(method_combo), "Subtract");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(method_combo), "Multiply");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(method_combo), "Divide");
    gtk_combo_box_set_active(GTK_COMBO_BOX(method_combo), 0);
    gtk_box_pack_start(GTK_BOX(hbox), method_combo, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(method_vbox), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    label = gtk_label_new("Argument 1:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    arg1_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), arg1_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(method_vbox), hbox, FALSE, FALSE, 0);
    
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    label = gtk_label_new("Argument 2:");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    arg2_entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), arg2_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(method_vbox), hbox, FALSE, FALSE, 0);
    
    calculate_btn = gtk_button_new_with_label("Calculate");
    g_signal_connect(calculate_btn, "clicked", G_CALLBACK(on_calculate_clicked), NULL);
    gtk_widget_set_sensitive(calculate_btn, FALSE);
    gtk_box_pack_start(GTK_BOX(method_vbox), calculate_btn, FALSE, FALSE, 0);
    
    result_label = gtk_label_new("Result: --");
    gtk_label_set_selectable(GTK_LABEL(result_label), TRUE);
    gtk_label_set_xalign(GTK_LABEL(result_label), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), result_label, FALSE, FALSE, 0);
    
    status_label = gtk_label_new("Status: Disconnected");
    gtk_label_set_xalign(GTK_LABEL(status_label), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, FALSE, 0);
    
    gtk_widget_show_all(window);
    
    gtk_main();
    
    return 0;
}
