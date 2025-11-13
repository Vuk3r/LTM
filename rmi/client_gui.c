#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include <pthread.h>
#include <time.h>

// Biến toàn cục
GtkWidget *ip_entry;
GtkWidget *port_entry;
GtkWidget *trip_entry;
GtkWidget *seat_entry;
GtkWidget *name_entry;
GtkWidget *log_text_view;
GtkWidget *status_label;
GtkWidget *book_button;

// Hàm thêm log vào text view
void append_log(const char *message) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_text_view));
    GtkTextIter iter;
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0';
    
    char log_line[1024];
    snprintf(log_line, sizeof(log_line), "[%s] %s\n", time_str, message);
    
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert(buffer, &iter, log_line, -1);
    
    GtkTextMark *mark = gtk_text_buffer_get_mark(buffer, "end");
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(log_text_view), mark, 0.0, FALSE, 0.0, 0.0);
}

// Wrapper functions cho g_idle_add
gboolean append_log_idle(gpointer data) {
    char *msg = (char*)data;
    append_log(msg);
    g_free(msg);
    return FALSE;
}

gboolean set_widget_sensitive_idle(gpointer data) {
    GtkWidget *widget = (GtkWidget*)data;
    gtk_widget_set_sensitive(widget, TRUE);
    return FALSE;
}

gboolean set_label_text_idle(gpointer data) {
    char **args = (char**)data;
    gtk_label_set_text(GTK_LABEL(status_label), args[0]);
    g_free(args[0]);
    g_free(args);
    return FALSE;
}

// Hàm xử lý đặt vé trong thread riêng
void *book_ticket_thread(void *data) {
    char *server_ip = ((char**)data)[0];
    int port = *((int*)((char**)data)[1]);
    char *trip_id = ((char**)data)[2];
    int seat_number = *((int*)((char**)data)[3]);
    char *passenger_name = ((char**)data)[4];

    int sock;
    struct sockaddr_in server;
    char message[1000], server_reply[2000];

    char log_msg[512];
    snprintf(log_msg, sizeof(log_msg), "Đang kết nối tới server %s:%d...", server_ip, port);
    g_idle_add(append_log_idle, g_strdup(log_msg));

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[ERROR] Không thể tạo socket: %s", strerror(errno));
        g_idle_add(append_log_idle, g_strdup(err_msg));
        g_idle_add(set_widget_sensitive_idle, book_button);
        char **label_data = g_malloc(sizeof(char*));
        label_data[0] = g_strdup("Lỗi: Không thể tạo socket");
        g_idle_add(set_label_text_idle, label_data);
        free(server_ip);
        free(trip_id);
        free(passenger_name);
        free(data);
        return NULL;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    
    if (inet_aton(server_ip, &server.sin_addr) == 0) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[ERROR] Địa chỉ IP không hợp lệ: %s", server_ip);
        g_idle_add(append_log_idle, g_strdup(err_msg));
        close(sock);
        g_idle_add(set_widget_sensitive_idle, book_button);
        char **label_data = g_malloc(sizeof(char*));
        label_data[0] = g_strdup("Lỗi: IP không hợp lệ");
        g_idle_add(set_label_text_idle, label_data);
        free(server_ip);
        free(trip_id);
        free(passenger_name);
        free(data);
        return NULL;
    }

    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[ERROR] Kết nối thất bại: %s", strerror(errno));
        g_idle_add(append_log_idle, g_strdup(err_msg));
        close(sock);
        g_idle_add(set_widget_sensitive_idle, book_button);
        char **label_data = g_malloc(sizeof(char*));
        label_data[0] = g_strdup("Lỗi: Không thể kết nối tới server");
        g_idle_add(set_label_text_idle, label_data);
        free(server_ip);
        free(trip_id);
        free(passenger_name);
        free(data);
        return NULL;
    }
    
    g_idle_add(append_log_idle, g_strdup("Đã kết nối thành công tới server"));

    snprintf(message, sizeof(message), "BOOK|%s|%d|%s", trip_id, seat_number, passenger_name);
    
    if (send(sock, message, strlen(message), 0) < 0) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[ERROR] Gửi thất bại: %s", strerror(errno));
        g_idle_add(append_log_idle, g_strdup(err_msg));
        close(sock);
        g_idle_add(set_widget_sensitive_idle, book_button);
        char **label_data = g_malloc(sizeof(char*));
        label_data[0] = g_strdup("Lỗi: Gửi yêu cầu thất bại");
        g_idle_add(set_label_text_idle, label_data);
        free(server_ip);
        free(trip_id);
        free(passenger_name);
        free(data);
        return NULL;
    }

    g_idle_add(append_log_idle, g_strdup("Đã gửi yêu cầu đặt vé"));

    memset(server_reply, 0, sizeof(server_reply));
    int recv_size = recv(sock, server_reply, sizeof(server_reply) - 1, 0);
    if (recv_size < 0) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[ERROR] Recv thất bại: %s", strerror(errno));
        g_idle_add(append_log_idle, g_strdup(err_msg));
        close(sock);
        g_idle_add(set_widget_sensitive_idle, book_button);
        char **label_data = g_malloc(sizeof(char*));
        label_data[0] = g_strdup("Lỗi: Nhận phản hồi thất bại");
        g_idle_add(set_label_text_idle, label_data);
        free(server_ip);
        free(trip_id);
        free(passenger_name);
        free(data);
        return NULL;
    } else if (recv_size == 0) {
        g_idle_add(append_log_idle, g_strdup("[ERROR] Server đóng kết nối"));
        close(sock);
        g_idle_add(set_widget_sensitive_idle, book_button);
        char **label_data = g_malloc(sizeof(char*));
        label_data[0] = g_strdup("Lỗi: Server đóng kết nối");
        g_idle_add(set_label_text_idle, label_data);
        free(server_ip);
        free(trip_id);
        free(passenger_name);
        free(data);
        return NULL;
    } else {
        server_reply[recv_size] = '\0';
        char result_msg[512];
        snprintf(result_msg, sizeof(result_msg), "Phản hồi từ server: %s", server_reply);
        g_idle_add(append_log_idle, g_strdup(result_msg));
        
        if (strncmp(server_reply, "OK:", 3) == 0) {
            char success_msg[512];
            snprintf(success_msg, sizeof(success_msg), "✓ Đặt vé thành công cho '%s'!", passenger_name);
            g_idle_add(append_log_idle, g_strdup(success_msg));
            char **label_data = g_malloc(sizeof(char*));
            label_data[0] = g_strdup("Đặt vé thành công!");
            g_idle_add(set_label_text_idle, label_data);
        } else {
            char fail_msg[512];
            snprintf(fail_msg, sizeof(fail_msg), "✗ Đặt vé thất bại: %s", server_reply);
            g_idle_add(append_log_idle, g_strdup(fail_msg));
            char **label_data = g_malloc(sizeof(char*));
            label_data[0] = g_strdup("Đặt vé thất bại");
            g_idle_add(set_label_text_idle, label_data);
        }
    }

    close(sock);
    g_idle_add(set_widget_sensitive_idle, book_button);
    
    free(server_ip);
    free(trip_id);
    free(passenger_name);
    free(data);
    return NULL;
}


// Callback khi nhấn nút Đặt vé
void on_book_clicked(GtkWidget *widget, gpointer data) {
    const char *server_ip = gtk_entry_get_text(GTK_ENTRY(ip_entry));
    const char *port_str = gtk_entry_get_text(GTK_ENTRY(port_entry));
    const char *trip_id = gtk_entry_get_text(GTK_ENTRY(trip_entry));
    const char *seat_str = gtk_entry_get_text(GTK_ENTRY(seat_entry));
    const char *passenger_name = gtk_entry_get_text(GTK_ENTRY(name_entry));

    // Kiểm tra input
    if (strlen(server_ip) == 0 || strlen(port_str) == 0 || 
        strlen(trip_id) == 0 || strlen(seat_str) == 0 || strlen(passenger_name) == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Lỗi: Vui lòng điền đầy đủ thông tin");
        return;
    }

    int port = atoi(port_str);
    if (port <= 0 || port > 65535) {
        gtk_label_set_text(GTK_LABEL(status_label), "Lỗi: Port không hợp lệ (1-65535)");
        return;
    }

    int seat_number = atoi(seat_str);
    if (seat_number <= 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Lỗi: Số ghế không hợp lệ");
        return;
    }

    // Vô hiệu hóa nút trong khi xử lý
    gtk_widget_set_sensitive(book_button, FALSE);
    gtk_label_set_text(GTK_LABEL(status_label), "Đang xử lý...");

    // Chuẩn bị dữ liệu cho thread
    char **thread_data = malloc(5 * sizeof(char*));
    thread_data[0] = strdup(server_ip);
    thread_data[1] = malloc(sizeof(int));
    *((int*)thread_data[1]) = port;
    thread_data[2] = strdup(trip_id);
    thread_data[3] = malloc(sizeof(int));
    *((int*)thread_data[3]) = seat_number;
    thread_data[4] = strdup(passenger_name);

    // Tạo thread để xử lý
    pthread_t thread;
    if (pthread_create(&thread, NULL, book_ticket_thread, thread_data) != 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Lỗi: Không thể tạo thread");
        gtk_widget_set_sensitive(book_button, TRUE);
        free(thread_data[0]);
        free(thread_data[1]);
        free(thread_data[2]);
        free(thread_data[3]);
        free(thread_data[4]);
        free(thread_data);
        return;
    }
    pthread_detach(thread);
}

// Callback khi đóng cửa sổ
gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
    gtk_main_quit();
    return FALSE;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Tạo cửa sổ chính
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "RMI Client - Đặt vé");
    gtk_window_set_default_size(GTK_WINDOW(window), 500, 500);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_delete), NULL);

    // VBox chính
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    // Frame thông tin server
    GtkWidget *server_frame = gtk_frame_new("Thông tin Server");
    gtk_box_pack_start(GTK_BOX(vbox), server_frame, FALSE, FALSE, 0);

    GtkWidget *server_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(server_frame), server_box);
    gtk_container_set_border_width(GTK_CONTAINER(server_box), 10);

    GtkWidget *ip_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(server_box), ip_box, FALSE, FALSE, 0);
    GtkWidget *ip_label = gtk_label_new("IP Server:");
    gtk_box_pack_start(GTK_BOX(ip_box), ip_label, FALSE, FALSE, 0);
    ip_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ip_entry), "192.168.1.100");
    gtk_box_pack_start(GTK_BOX(ip_box), ip_entry, TRUE, TRUE, 0);

    GtkWidget *port_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(server_box), port_box, FALSE, FALSE, 0);
    GtkWidget *port_label = gtk_label_new("Port:");
    gtk_box_pack_start(GTK_BOX(port_box), port_label, FALSE, FALSE, 0);
    port_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(port_entry), "9999");
    gtk_box_pack_start(GTK_BOX(port_box), port_entry, FALSE, FALSE, 0);

    // Frame thông tin đặt vé
    GtkWidget *booking_frame = gtk_frame_new("Thông tin đặt vé");
    gtk_box_pack_start(GTK_BOX(vbox), booking_frame, FALSE, FALSE, 0);

    GtkWidget *booking_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(booking_frame), booking_box);
    gtk_container_set_border_width(GTK_CONTAINER(booking_box), 10);

    GtkWidget *trip_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(booking_box), trip_box, FALSE, FALSE, 0);
    GtkWidget *trip_label = gtk_label_new("Trip ID:");
    gtk_box_pack_start(GTK_BOX(trip_box), trip_label, FALSE, FALSE, 0);
    trip_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(trip_entry), "TRIP01");
    gtk_box_pack_start(GTK_BOX(trip_box), trip_entry, TRUE, TRUE, 0);

    GtkWidget *seat_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(booking_box), seat_box, FALSE, FALSE, 0);
    GtkWidget *seat_label = gtk_label_new("Số ghế:");
    gtk_box_pack_start(GTK_BOX(seat_box), seat_label, FALSE, FALSE, 0);
    seat_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(seat_entry), "5");
    gtk_box_pack_start(GTK_BOX(seat_box), seat_entry, FALSE, FALSE, 0);

    GtkWidget *name_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(booking_box), name_box, FALSE, FALSE, 0);
    GtkWidget *name_label = gtk_label_new("Tên hành khách:");
    gtk_box_pack_start(GTK_BOX(name_box), name_label, FALSE, FALSE, 0);
    name_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(name_entry), "Nguyen Van A");
    gtk_box_pack_start(GTK_BOX(name_box), name_entry, TRUE, TRUE, 0);

    // Nút đặt vé
    book_button = gtk_button_new_with_label("Đặt vé");
    g_signal_connect(book_button, "clicked", G_CALLBACK(on_book_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), book_button, FALSE, FALSE, 0);

    // Status label
    status_label = gtk_label_new("Sẵn sàng");
    gtk_label_set_xalign(GTK_LABEL(status_label), 0.0);
    gtk_box_pack_start(GTK_BOX(vbox), status_label, FALSE, FALSE, 0);

    // Log frame
    GtkWidget *log_frame = gtk_frame_new("Log");
    gtk_box_pack_start(GTK_BOX(vbox), log_frame, TRUE, TRUE, 0);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), 
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(log_frame), scrolled_window);

    log_text_view = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(log_text_view), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(log_text_view), TRUE);
    gtk_container_add(GTK_CONTAINER(scrolled_window), log_text_view);

    gtk_widget_show_all(window);
    append_log("Client GUI đã sẵn sàng. Nhập thông tin để đặt vé.");

    gtk_main();
    return 0;
}

