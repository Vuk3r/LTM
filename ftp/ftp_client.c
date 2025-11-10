#include "ftp_client.h"

static int connect_to_server(const char *ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }
    
    return sock;
}

static int read_response(int fd, char *buffer, size_t size) {
    int n = recv(fd, buffer, size - 1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        return n;
    }
    return -1;
}

static int send_command(int fd, const char *cmd, const char *arg) {
    char buffer[512];
    if (arg && strlen(arg) > 0) {
        snprintf(buffer, sizeof(buffer), "%s %s\r\n", cmd, arg);
    } else {
        snprintf(buffer, sizeof(buffer), "%s\r\n", cmd);
    }
    return send(fd, buffer, strlen(buffer), 0);
}

ftp_client_t *ftp_client_new(void) {
    ftp_client_t *client = malloc(sizeof(ftp_client_t));
    memset(client, 0, sizeof(ftp_client_t));
    client->control_fd = -1;
    client->data_fd = -1;
    client->passive_fd = -1;
    client->authenticated = 0;
    return client;
}

void ftp_client_free(ftp_client_t *client) {
    if (client->control_fd >= 0) close(client->control_fd);
    if (client->data_fd >= 0) close(client->data_fd);
    if (client->passive_fd >= 0) close(client->passive_fd);
    free(client);
}

int ftp_client_connect(ftp_client_t *client, const char *ip, int port) {
    client->control_fd = connect_to_server(ip, port);
    if (client->control_fd < 0) return -1;
    
    strncpy(client->server_ip, ip, sizeof(client->server_ip) - 1);
    client->server_port = port;
    
    char response[512];
    read_response(client->control_fd, response, sizeof(response));
    return 0;
}

int ftp_client_login(ftp_client_t *client, const char *username, const char *password) {
    char response[512];
    
    send_command(client->control_fd, CMD_USER, username);
    read_response(client->control_fd, response, sizeof(response));
    
    send_command(client->control_fd, CMD_PASS, password);
    read_response(client->control_fd, response, sizeof(response));
    
    if (strstr(response, "230") != NULL) {
        client->authenticated = 1;
        return 0;
    }
    return -1;
}

int ftp_client_pasv(ftp_client_t *client) {
    char response[512];
    send_command(client->control_fd, CMD_PASV, NULL);
    read_response(client->control_fd, response, sizeof(response));
    
    // Parse PASV response: "227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)"
    int h1, h2, h3, h4, p1, p2;
    if (sscanf(response, "%*d %*[^(](%d,%d,%d,%d,%d,%d)", &h1, &h2, &h3, &h4, &p1, &p2) == 6) {
        int data_port = p1 * 256 + p2;
        char data_ip[16];
        snprintf(data_ip, sizeof(data_ip), "%d.%d.%d.%d", h1, h2, h3, h4);
        
        client->data_fd = connect_to_server(data_ip, data_port);
        if (client->data_fd >= 0) {
            return 0;
        }
    }
    return -1;
}

int ftp_client_list(ftp_client_t *client, char *buffer, size_t buffer_size) {
    if (!client->authenticated) return -1;
    
    if (ftp_client_pasv(client) < 0) return -1;
    
    char response[512];
    send_command(client->control_fd, CMD_LIST, NULL);
    read_response(client->control_fd, response, sizeof(response));
    
    if (client->data_fd >= 0) {
        int total = 0;
        int n;
        buffer[0] = '\0';
        while ((n = recv(client->data_fd, buffer + total, buffer_size - total - 1, 0)) > 0) {
            total += n;
            if (total >= buffer_size - 1) break;
        }
        buffer[total] = '\0';
        close(client->data_fd);
        client->data_fd = -1;
        
        read_response(client->control_fd, response, sizeof(response));
        return total;
    }
    return -1;
}

int ftp_client_retr(ftp_client_t *client, const char *remote_file, const char *local_file) {
    if (!client->authenticated) return -1;
    
    if (ftp_client_pasv(client) < 0) return -1;
    
    char response[512];
    send_command(client->control_fd, CMD_RETR, remote_file);
    read_response(client->control_fd, response, sizeof(response));
    
    if (client->data_fd >= 0) {
        FILE *file = fopen(local_file, "wb");
        if (!file) {
            close(client->data_fd);
            client->data_fd = -1;
            return -1;
        }
        
        char buffer[BUFFER_SIZE];
        int n;
        while ((n = recv(client->data_fd, buffer, sizeof(buffer), 0)) > 0) {
            fwrite(buffer, 1, n, file);
        }
        fclose(file);
        close(client->data_fd);
        client->data_fd = -1;
        
        read_response(client->control_fd, response, sizeof(response));
        return 0;
    }
    return -1;
}

int ftp_client_stor(ftp_client_t *client, const char *local_file, const char *remote_file) {
    if (!client->authenticated) return -1;
    
    FILE *file = fopen(local_file, "rb");
    if (!file) return -1;
    
    if (ftp_client_pasv(client) < 0) {
        fclose(file);
        return -1;
    }
    
    char response[512];
    send_command(client->control_fd, CMD_STOR, remote_file);
    read_response(client->control_fd, response, sizeof(response));
    
    if (client->data_fd >= 0) {
        char buffer[BUFFER_SIZE];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), file)) > 0) {
            send(client->data_fd, buffer, n, 0);
        }
        fclose(file);
        close(client->data_fd);
        client->data_fd = -1;
        
        read_response(client->control_fd, response, sizeof(response));
        return 0;
    }
    fclose(file);
    return -1;
}

int ftp_client_cwd(ftp_client_t *client, const char *dir) {
    if (!client->authenticated) return -1;
    
    char response[512];
    send_command(client->control_fd, CMD_CWD, dir);
    read_response(client->control_fd, response, sizeof(response));
    
    if (strstr(response, "250") != NULL) {
        return 0;
    }
    return -1;
}

int ftp_client_pwd(ftp_client_t *client, char *buffer, size_t size) {
    if (!client->authenticated) return -1;
    
    char response[512];
    send_command(client->control_fd, CMD_PWD, NULL);
    read_response(client->control_fd, response, sizeof(response));
    
    // Parse PWD response: "257 "/path""
    if (sscanf(response, "%*d %*[^\"]\"%[^\"]\"", buffer) == 1) {
        return 0;
    }
    return -1;
}

int ftp_client_quit(ftp_client_t *client) {
    char response[512];
    send_command(client->control_fd, CMD_QUIT, NULL);
    read_response(client->control_fd, response, sizeof(response));
    return 0;
}

