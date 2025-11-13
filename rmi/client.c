#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Cách dùng: %s <IP_SERVER> <PORT> <TRIP_ID> <SEAT_NUMBER> <Tên hành khách>\n", argv[0]);
        fprintf(stderr, "Ví dụ: %s 192.168.1.100 9999 TRIP01 5 \"Nguyen Van A\"\n", argv[0]);
        return 1;
    }
    
    char *server_ip = argv[1];
    int port = atoi(argv[2]);
    char *trip_id = argv[3];
    int seat_number = atoi(argv[4]);
    char *passenger_name = argv[5];

    if (port <= 0 || port > 65535) {
        fprintf(stderr, "[ERROR] Port không hợp lệ (phải từ 1-65535)\n");
        return 1;
    }

    if (seat_number <= 0) {
        fprintf(stderr, "[ERROR] Số ghế không hợp lệ\n");
        return 1;
    }

    printf("Client đang kết nối tới server %s:%d\n", server_ip, port);
    printf("Đặt vé: Trip=%s, Ghế=%d, Hành khách=%s\n", trip_id, seat_number, passenger_name);

    int sock;
    struct sockaddr_in server;
    char message[1000], server_reply[2000];

    // Tạo socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("[ERROR] Không thể tạo socket");
        return 1;
    }

    // Cấu hình địa chỉ server
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(port);
    
    if (inet_aton(server_ip, &server.sin_addr) == 0) {
        fprintf(stderr, "[ERROR] Địa chỉ IP không hợp lệ: %s\n", server_ip);
        close(sock);
        return 1;
    }

    // Kết nối tới server
    if (connect(sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
        perror("[ERROR] Kết nối thất bại");
        close(sock);
        return 1;
    }
    
    printf("Đã kết nối thành công tới server\n");

    // Tạo request: BOOK|TRIP_ID|SEAT|NAME
    sprintf(message, "BOOK|%s|%d|%s", trip_id, seat_number, passenger_name);
    
    // Gửi "remote call"
    if (send(sock, message, strlen(message), 0) < 0) {
        perror("[ERROR] Gửi thất bại");
        close(sock);
        return 1;
    }

    printf("Đã gửi yêu cầu đặt vé\n");

    // Nhận phản hồi
    memset(server_reply, 0, sizeof(server_reply));
    int recv_size = recv(sock, server_reply, sizeof(server_reply) - 1, 0);
    if (recv_size < 0) {
        perror("[ERROR] Recv thất bại");
        close(sock);
        return 1;
    } else if (recv_size == 0) {
        fprintf(stderr, "[ERROR] Server đóng kết nối\n");
        close(sock);
        return 1;
    } else {
        server_reply[recv_size] = '\0'; // Đảm bảo kết thúc chuỗi
        printf("Hành khách '%s' nhận phản hồi: %s\n", passenger_name, server_reply);
        
        if (strncmp(server_reply, "OK:", 3) == 0) {
            printf("✓ Đặt vé thành công!\n");
        } else {
            printf("✗ Đặt vé thất bại: %s\n", server_reply);
        }
    }

    close(sock);
    return 0;
}

