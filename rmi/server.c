#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sqlite3.h>

// --- Biến toàn cục ---
sqlite3 *db;
// Đây chính là "synchronized" của C
pthread_mutex_t db_mutex = PTHREAD_MUTEX_INITIALIZER; 

// --- Hàm xử lý logic đặt vé ---
// Trả về một chuỗi phản hồi (cần được free sau khi gửi)
char* book_seat(const char* tripId, int seat, const char* name) {
    sqlite3_stmt *stmt;
    char sql_check[256];
    char sql_insert[256];
    char *response;

    // ----- BẮT ĐẦU VÙNG QUAN TRỌNG (CRITICAL SECTION) -----
    // Yêu cầu "chìa khóa". Các luồng khác phải đợi ở đây.
    pthread_mutex_lock(&db_mutex);

    printf("[Thread %lu] Đang xử lý ghế %d cho '%s'...\n", (unsigned long)pthread_self(), seat, name);

    // 1. KIỂM TRA (SELECT)
    sprintf(sql_check, "SELECT COUNT(*) FROM Bookings WHERE tripId = ?1 AND seatNumber = ?2;");
    
    if (sqlite3_prepare_v2(db, sql_check, -1, &stmt, 0) != SQLITE_OK) {
        fprintf(stderr, "[ERROR] Lỗi prepare DB: %s\n", sqlite3_errmsg(db));
        response = strdup("ERROR: Lỗi prepare DB");
        sqlite3_finalize(stmt);
        pthread_mutex_unlock(&db_mutex); // Nhớ unlock trước khi thoát
        return response;
    }
    
    sqlite3_bind_text(stmt, 1, tripId, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 2, seat);

    int seat_count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        seat_count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);

    // 2. KIỂM TRA LOGIC
    if (seat_count > 0) {
        // Ghế đã bị đặt!
        printf("[Thread %lu] Ghế %d THẤT BẠI (đã có người đặt)\n", (unsigned long)pthread_self(), seat);
        response = strdup("ERROR:SEAT_TAKEN");
    } else {
        // Ghế trống, tiến hành đặt
        
        // Giả lập thời gian xử lý (để cho các luồng khác có cơ hội chen vào nếu ta không dùng mutex)
        sleep(1); 

        // 3. HÀNH ĐỘNG (INSERT)
        sprintf(sql_insert, "INSERT INTO Bookings (tripId, seatNumber, passengerName) VALUES (?1, ?2, ?3);");
        
        if (sqlite3_prepare_v2(db, sql_insert, -1, &stmt, 0) != SQLITE_OK) {
            fprintf(stderr, "[ERROR] Lỗi prepare insert: %s\n", sqlite3_errmsg(db));
            response = strdup("ERROR: Lỗi prepare insert");
        } else {
            sqlite3_bind_text(stmt, 1, tripId, -1, SQLITE_STATIC);
            sqlite3_bind_int(stmt, 2, seat);
            sqlite3_bind_text(stmt, 3, name, -1, SQLITE_STATIC);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                fprintf(stderr, "[ERROR] Insert thất bại: %s\n", sqlite3_errmsg(db));
                response = strdup("ERROR: Insert thất bại");
            } else {
                printf("[Thread %lu] Ghế %d THÀNH CÔNG cho '%s'\n", (unsigned long)pthread_self(), seat, name);
                response = strdup("OK:BOOKED");
            }
        }
        sqlite3_finalize(stmt);
    }

    // Trả "chìa khóa" cho luồng tiếp theo
    pthread_mutex_unlock(&db_mutex);
    // ----- KẾT THÚC VÙNG QUAN TRỌNG -----
    
    return response;
}

// --- Hàm xử lý cho mỗi kết nối (mỗi client) ---
void *connection_handler(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char client_message[2000], *response;
    int read_size;

    // Nhận message từ client
    // Giao thức đơn giản: BOOK|TRIP_ID|SEAT|NAME
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
            fprintf(stderr, "[ERROR] Lệnh không hợp lệ từ client\n");
            response = strdup("ERROR:INVALID_COMMAND");
        }

        // Gửi phản hồi lại client
        if (write(sock, response, strlen(response)) < 0) {
            perror("[ERROR] Gửi phản hồi thất bại");
        }
        free(response); // Giải phóng chuỗi đã `strdup`
        
        memset(client_message, 0, 2000);
    }

    if(read_size == 0) {
        puts("Client ngắt kết nối");
    } else if(read_size == -1) {
        perror("[ERROR] recv failed");
    }

    close(sock);
    free(socket_desc); // Giải phóng con trỏ socket
    return 0;
}

// --- Hàm Main ---
int main(int argc, char *argv[]) {
    int socket_desc, client_sock, c;
    struct sockaddr_in server, client;
    int port;
    char *ip_address;

    // Kiểm tra tham số dòng lệnh
    if (argc != 3) {
        fprintf(stderr, "Cách dùng: %s <IP_LAN> <PORT>\n", argv[0]);
        fprintf(stderr, "Ví dụ: %s 192.168.1.100 9999\n", argv[0]);
        return 1;
    }

    ip_address = argv[1];
    port = atoi(argv[2]);

    if (port <= 0 || port > 65535) {
        fprintf(stderr, "[ERROR] Port không hợp lệ (phải từ 1-65535)\n");
        return 1;
    }

    printf("Server đang khởi động với IP: %s, Port: %d\n", ip_address, port);

    // 1. Mở và khởi tạo Database
    if (sqlite3_open("ticket_system.db", &db)) {
        fprintf(stderr, "[ERROR] Không thể mở database: %s\n", sqlite3_errmsg(db));
        return 1;
    }
    printf("Database đã mở thành công\n");

    char *err_msg = 0;
    const char *sql_create = "CREATE TABLE IF NOT EXISTS Bookings ("
                             "tripId TEXT, "
                             "seatNumber INTEGER, "
                             "passengerName TEXT, "
                             "PRIMARY KEY (tripId, seatNumber));";
    if (sqlite3_exec(db, sql_create, 0, 0, &err_msg) != SQLITE_OK) {
        fprintf(stderr, "[ERROR] Lỗi SQL: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(db);
        return 1;
    }
    printf("Bảng Bookings đã được tạo/kiểm tra\n");

    // 2. Tạo Socket
    socket_desc = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_desc == -1) {
        perror("[ERROR] Không thể tạo socket");
        sqlite3_close(db);
        return 1;
    }
    puts("Socket đã tạo");

    // Cho phép tái sử dụng địa chỉ
    int opt = 1;
    if (setsockopt(socket_desc, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[ERROR] setsockopt thất bại");
        close(socket_desc);
        sqlite3_close(db);
        return 1;
    }

    // 3. Cấu hình Server Address
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    if (inet_aton(ip_address, &server.sin_addr) == 0) {
        fprintf(stderr, "[ERROR] Địa chỉ IP không hợp lệ: %s\n", ip_address);
        close(socket_desc);
        sqlite3_close(db);
        return 1;
    }
    server.sin_port = htons(port);

    // 4. Bind
    if (bind(socket_desc, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("[ERROR] Bind thất bại");
        close(socket_desc);
        sqlite3_close(db);
        return 1;
    }
    printf("Bind thành công tại %s:%d\n", ip_address, port);

    // 5. Listen
    if (listen(socket_desc, 3) < 0) {
        perror("[ERROR] Listen thất bại");
        close(socket_desc);
        sqlite3_close(db);
        return 1;
    }
    printf("Server đang chờ kết nối tại %s:%d...\n", ip_address, port);
    c = sizeof(struct sockaddr_in);

    // 6. Vòng lặp Accept và tạo luồng
    while ((client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c))) {
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Kết nối được chấp nhận từ %s:%d\n", client_ip, ntohs(client.sin_port));
        
        pthread_t thread_id;
        int *new_sock = malloc(sizeof(int)); // Cần malloc vì đây là con trỏ
        if (new_sock == NULL) {
            fprintf(stderr, "[ERROR] Không thể cấp phát bộ nhớ\n");
            close(client_sock);
            continue;
        }
        *new_sock = client_sock;

        if (pthread_create(&thread_id, NULL, connection_handler, (void*) new_sock) < 0) {
            perror("[ERROR] Không thể tạo luồng");
            free(new_sock);
            close(client_sock);
            continue;
        }
        
        // Tách luồng để không chờ nó kết thúc
        pthread_detach(thread_id);
    }

    if (client_sock < 0) {
        perror("[ERROR] Accept thất bại");
        close(socket_desc);
        sqlite3_close(db);
        return 1;
    }
    
    close(socket_desc);
    sqlite3_close(db);
    return 0;
}

