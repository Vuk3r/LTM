#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sqlite3.h>
#include <gtk/gtk.h>
#include <time.h>

// --- Biến toàn cục ---
sqlite3 *db;
pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER;
GtkWidget *log_text_view;
GtkWidget *status_label;
GtkWidget *start_button;
GtkWidget *stop_button;
GtkWidget *ip_entry;
GtkWidget *port_entry;
int server_running = 0;
int socket_desc = -1;
pthread_t server_thread;

// Hàm thêm log vào text view (thread-safe)
void append_log(const char *message) {
    GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(log_text_view));
    GtkTextIter iter;
    time_t now = time(NULL);
    char *time_str = ctime(&now);
    time_str[strlen(time_str) - 1] = '\0'; // Bỏ newline
    
    char log_line[1024];
    snprintf(log_line, sizeof(log_line), "[%s] %s\n", time_str, message);
    
    gtk_text_buffer_get_end_iter(buffer, &iter);
    gtk_text_buffer_insert(buffer, &iter, log_line, -1);
    
    // Auto scroll to bottom
    GtkTextMark *mark = gtk_text_buffer_get_mark(buffer, "end");
    gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(log_text_view), mark, 0.0, FALSE, 0.0, 0.0);
}

// Wrapper function cho g_idle_add
gboolean append_log_idle(gpointer data) {
    char *msg = (char*)data;
    append_log(msg);
    g_free(msg);
    return FALSE;
}

// Hàm xử lý logic đặt vé
char* book_seat(const char* tripId, int seat, const char* name) {
    sqlite3_stmt *stmt;
    char sql_check[256];
    char sql_insert[256];
    char *response;
    char log_msg[512];

    pthread_mutex_lock(&db_mutex);

    snprintf(log_msg, sizeof(log_msg), "[Thread %lu] Đang xử lý ghế %d cho '%s'...", 
             (unsigned long)pthread_self(), seat, name);
    g_idle_add((GSourceFunc)append_log_idle, g_strdup(log_msg));

    sprintf(sql_check, "SELECT COUNT(*) FROM Bookings WHERE tripId = ?1 AND seatNumber = ?2;");
    
    if (sqlite3_prepare_v2(db, sql_check, -1, &stmt, 0) != SQLITE_OK) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[ERROR] Lỗi prepare DB: %s", sqlite3_errmsg(db));
        g_idle_add(append_log_idle, g_strdup(err_msg));
        response = strdup("ERROR: Lỗi prepare DB");
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex);
        return response;
    }
    
    sqlite3_bind_text(stmt, 1, tripId, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, seat);

    int seat_count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        seat_count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    if (seat_count > 0) {
        snprintf(log_msg, sizeof(log_msg), "[Thread %lu] Ghế %d THẤT BẠI (đã có người đặt)", 
                 (unsigned long)pthread_self(), seat);
        g_idle_add(append_log_idle, g_strdup(log_msg));
        response = strdup("ERROR:SEAT_TAKEN");
    } else {
        sleep(1);

        sprintf(sql_insert, "INSERT INTO Bookings (tripId, seatNumber, passengerName) VALUES (?1, ?2, ?3);");
        
        if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0) != SQLITE_OK) {
            char err_msg[256];
            snprintf(err_msg, sizeof(err_msg), "[ERROR] Lỗi prepare insert: %s", sqlite3_errmsg(db));
            g_idle_add(append_log_idle, g_strdup(err_msg));
            response = strdup("ERROR: Lỗi prepare insert");
        } else {
            sqlite3_bind_text(stmt, 1, tripId, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, seat);
            sqlite3_bind_text(stmt, 3, name, -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                char err_msg[256];
                snprintf(err_msg, sizeof(err_msg), "[ERROR] Insert thất bại: %s", sqlite3_errmsg(db));
                g_idle_add(append_log_idle, g_strdup(err_msg));
                response = strdup("ERROR: Insert thất bại");
            } else {
                snprintf(log_msg, sizeof(log_msg), "[Thread %lu] Ghế %d THÀNH CÔNG cho '%s'", 
                         (unsigned long)pthread_self(), seat, name);
                g_idle_add(append_log_idle, g_strdup(log_msg));
                response = strdup("OK:BOOKED");
            }
        }
        sqlite3_finalize(stmt);
    }

    pthread_mutex_unlock(&db_mutex);
    return response;
}

// Hàm xử lý cho mỗi kết nối
void *connection_handler(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char client_message[2000], *response;
    int read_size;

    while ((read_size = recv(sock, client_message, 2000, 0)) > 0) {
        client_message[read_size] = '\0';
        
        char *cmd = strtok(client_message, "|");
        char *trip = strtok(NULL, "|");
        char *seat_str = strtok(NULL, "|");
        char *name = strtok(NULL, "|");

        if (cmd && trip && seat_str && name && strcmp(cmd, "BOOK") == 0) {
            int seat = atoi(seat_str);
            response = book_seat(trip, seat, name);
        } else {
            g_idle_add(append_log_idle, g_strdup("[ERROR] Lệnh không hợp lệ từ client"));
            response = strdup("ERROR:INVALID_COMMAND");
        }

        if (write(sock, response, strlen(response)) < 0) {
            g_idle_add(append_log_idle, g_strdup("[ERROR] Gửi phản hồi thất bại"));
        }
        free(response);
        
        memset(client_message, 0, 2000);
    }

    if(read_size == 0) {
        g_idle_add(append_log_idle, g_strdup("Client ngắt kết nối"));
    } else if(read_size == -1) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[ERROR] recv failed: %s", strerror(errno));
        g_idle_add(append_log_idle, g_strdup(err_msg));
    }

    close(sock);
    free(socket_desc);
    return 0;
}

// Thread chạy server
void *server_thread_func(void *arg) {
    struct sockaddr_in server, client;
    int client_sock, c;
    char *ip_address = (char*)arg;
    int port = *((int*)((char*)arg + strlen(ip_address) + 1));

    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[ERROR] Không thể tạo socket: %s", strerror(errno));
        g_idle_add(append_log_idle, g_strdup(err_msg));
        server_running = 0;
        return NULL;
    }

    int opt = 1;
    if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[ERROR] setsockopt thất bại: %s", strerror(errno));
        g_idle_add(append_log_idle, g_strdup(err_msg));
        close(socket_desc);
        server_running = 0;
        return NULL;
    }

    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    if (inet_aton(ip_address, &server.sin_addr) == 0) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[ERROR] Địa chỉ IP không hợp lệ: %s", ip_address);
        g_idle_add(append_log_idle, g_strdup(err_msg));
        close(socket_desc);
        server_running = 0;
        return NULL;
    }
    server.sin_port = htons(port);

    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[ERROR] Bind thất bại: %s", strerror(errno));
        g_idle_add(append_log_idle, g_strdup(err_msg));
        close(socket_desc);
        server_running = 0;
        return NULL;
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "Bind thành công tại %s:%d", ip_address, port);
    g_idle_add((GSourceFunc)append_log, g_strdup(msg));

    if (listen(socket_desc, 3) < 0) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[ERROR] Listen thất bại: %s", strerror(errno));
        g_idle_add(append_log_idle, g_strdup(err_msg));
        close(socket_desc);
        server_running = 0;
        return NULL;
    }

    snprintf(msg, sizeof(msg), "Server đang chờ kết nối tại %s:%d...", ip_address, port);
    g_idle_add((GSourceFunc)append_log, g_strdup(msg));
    c = sizeof(struct sockaddr_in);

    while (server_running) {
        client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c);
        if (client_sock < 0) {
            if (server_running) {
                char err_msg[256];
                snprintf(err_msg, sizeof(err_msg), "[ERROR] Accept thất bại: %s", strerror(errno));
                g_idle_add(append_log_idle, g_strdup(err_msg));
            }
            break;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client.sin_addr, client_ip, INET_ADDRSTRLEN);
        snprintf(msg, sizeof(msg), "Kết nối được chấp nhận từ %s:%d", client_ip, ntohs(client.sin_port));
        g_idle_add(append_log_idle, g_strdup(msg));
        
        pthread_t thread_id;
        int *new_sock = malloc(sizeof(int));
        if (new_sock == NULL) {
            g_idle_add(append_log_idle, g_strdup("[ERROR] Không thể cấp phát bộ nhớ"));
            close(client_sock);
            continue;
        }
        *new_sock = client_sock;

        if (pthread_create(&thread_id, NULL, connection_handler, (void*) new_sock) < 0) {
            g_idle_add(append_log_idle, g_strdup("[ERROR] Không thể tạo luồng"));
            free(new_sock);
            close(client_sock);
            continue;
        }
        
        pthread_detach(thread_id);
    }

    close(socket_desc);
    return NULL;
}

// Callback khi nhấn nút Start
void on_start_clicked(GtkWidget *widget, gpointer data) {
    if (server_running) {
        return;
    }

    const char *ip = gtk_entry_get_text(GTK_ENTRY(ip_entry));
    const char *port_str = gtk_entry_get_text(GTK_ENTRY(port_entry));

    if (strlen(ip) == 0 || strlen(port_str) == 0) {
        gtk_label_set_text(GTK_LABEL(status_label), "Lỗi: Vui lòng nhập IP và Port");
        return;
    }

    int port = atoi(port_str);
    if (port <= 0 || port > 65535) {
        gtk_label_set_text(GTK_LABEL(status_label), "Lỗi: Port không hợp lệ (1-65535)");
        return;
    }

    // Khởi tạo database
    if (sqlite3_open("ticket_system.db", &db)) {
        char err_msg[256];
        snprintf(err_msg, sizeof(err_msg), "[ERROR] Không thể mở database: %s", sqlite3_errmsg(db));
        append_log(err_msg);
        gtk_label_set_text(GTK_LABEL(status_label), "Lỗi: Không thể mở database");
        return;
    }

    char *err_msg = 0;
    const char *sql_create = "CREATE TABLE IF NOT EXISTS Bookings ("
                             "tripId TEXT, "
                             "seatNumber INTEGER, "
                             "passengerName TEXT, "
                             "PRIMARY KEY (tripId, seatNumber));";
    if (sqlite3_exec(db, sql_create, 0, 0, &err_msg) != SQLITE_OK) {
        char err[256];
        snprintf(err, sizeof(err), "[ERROR] Lỗi SQL: %s", err_msg);
        append_log(err);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        gtk_label_set_text(GTK_LABEL(status_label), "Lỗi: Không thể tạo bảng");
        return;
    }

    append_log("Database đã mở thành công");
    append_log("Bảng Bookings đã được tạo/kiểm tra");

    // Tạo thread server
    server_running = 1;
    char *thread_data = malloc(strlen(ip) + 1 + sizeof(int));
    strcpy(thread_data, ip);
    memcpy(thread_data + strlen(ip) + 1, &port, sizeof(int));

    if (pthread_create(&server_thread, NULL, server_thread_func, thread_data) != 0) {
        append_log("[ERROR] Không thể tạo thread server");
        server_running = 0;
        sqlite3_close(db);
        free(thread_data);
        return;
    }

    gtk_widget_set_sensitive(start_button, FALSE);
    gtk_widget_set_sensitive(ip_entry, FALSE);
    gtk_widget_set_sensitive(port_entry, FALSE);
    gtk_widget_set_sensitive(stop_button, TRUE);
    
    char status[256];
    snprintf(status, sizeof(status), "Server đang chạy tại %s:%d", ip, port);
    gtk_label_set_text(GTK_LABEL(status_label), status);
}

// Callback khi nhấn nút Stop
void on_stop_clicked(GtkWidget *widget, gpointer data) {
    if (!server_running) {
        return;
    }

    server_running = 0;
    if (socket_desc >= 0) {
        close(socket_desc);
        socket_desc = -1;
    }
    pthread_join(server_thread, NULL);

    if (db) {
        sqlite3_close(db);
        db = NULL;
    }

    append_log("Server đã dừng");

    gtk_widget_set_sensitive(start_button, TRUE);
    gtk_widget_set_sensitive(ip_entry, TRUE);
    gtk_widget_set_sensitive(port_entry, TRUE);
    gtk_widget_set_sensitive(stop_button, FALSE);
    gtk_label_set_text(GTK_LABEL(status_label), "Server đã dừng");
}

// Callback khi đóng cửa sổ
gboolean on_window_delete(GtkWidget *widget, GdkEvent *event, gpointer data) {
    if (server_running) {
        on_stop_clicked(NULL, NULL);
    }
    gtk_main_quit();
    return FALSE;
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    // Tạo cửa sổ chính
    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "RMI Server - Hệ thống đặt vé");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 500);
    g_signal_connect(window, "destroy", G_CALLBACK(on_window_delete), NULL);

    // VBox chính
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), vbox);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    // Frame cấu hình
    GtkWidget *config_frame = gtk_frame_new("Cấu hình Server");
    gtk_box_pack_start(GTK_BOX(vbox), config_frame, FALSE, FALSE, 0);

    GtkWidget *config_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_container_add(GTK_CONTAINER(config_frame), config_box);
    gtk_container_set_border_width(GTK_CONTAINER(config_box), 10);

    // IP Entry
    GtkWidget *ip_label = gtk_label_new("IP LAN:");
    gtk_box_pack_start(GTK_BOX(config_box), ip_label, FALSE, FALSE, 0);
    ip_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ip_entry), "192.168.1.100");
    gtk_box_pack_start(GTK_BOX(config_box), ip_entry, TRUE, TRUE, 0);

    // Port Entry
    GtkWidget *port_label = gtk_label_new("Port:");
    gtk_box_pack_start(GTK_BOX(config_box), port_label, FALSE, FALSE, 0);
    port_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(port_entry), "9999");
    gtk_box_pack_start(GTK_BOX(config_box), port_entry, FALSE, FALSE, 0);

    // Buttons
    GtkWidget *button_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_box_pack_start(GTK_BOX(vbox), button_box, FALSE, FALSE, 0);

    start_button = gtk_button_new_with_label("Bắt đầu Server");
    g_signal_connect(start_button, "clicked", G_CALLBACK(on_start_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), start_button, TRUE, TRUE, 0);

    stop_button = gtk_button_new_with_label("Dừng Server");
    g_signal_connect(stop_button, "clicked", G_CALLBACK(on_stop_clicked), NULL);
    gtk_widget_set_sensitive(stop_button, FALSE);
    gtk_box_pack_start(GTK_BOX(button_box), stop_button, TRUE, TRUE, 0);

    // Status label
    status_label = gtk_label_new("Chưa khởi động");
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
    append_log("Server GUI đã sẵn sàng. Nhập IP và Port để bắt đầu.");

    gtk_main();
    return 0;
}

