#include "ftp_common.h"

void send_response(int fd, int code, const char *message) {
    char response[512];
    snprintf(response, sizeof(response), "%d %s\r\n", code, message);
    send(fd, response, strlen(response), 0);
}

int parse_port_command(const char *cmd, struct sockaddr_in *addr) {
    int h1, h2, h3, h4, p1, p2;
    if (sscanf(cmd, "PORT %d,%d,%d,%d,%d,%d", &h1, &h2, &h3, &h4, &p1, &p2) == 6) {
        addr->sin_family = AF_INET;
        char ip[16];
        snprintf(ip, sizeof(ip), "%d.%d.%d.%d", h1, h2, h3, h4);
        inet_pton(AF_INET, ip, &addr->sin_addr);
        addr->sin_port = htons(p1 * 256 + p2);
        return 0;
    }
    return -1;
}

void get_file_list(const char *path, char *buffer, size_t buffer_size) {
    DIR *dir = opendir(path);
    if (!dir) {
        strcpy(buffer, "");
        return;
    }
    
    buffer[0] = '\0';
    struct dirent *entry;
    struct stat file_stat;
    char full_path[MAX_PATH_LEN];
    char time_str[64];
    time_t now = time(NULL);
    
    while ((entry = readdir(dir)) != NULL && strlen(buffer) < buffer_size - 256) {
        if (entry->d_name[0] == '.') continue;
        
        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (stat(full_path, &file_stat) == 0) {
            struct tm *tm_info = localtime(&file_stat.st_mtime);
            strftime(time_str, sizeof(time_str), "%b %d %H:%M", tm_info);
            
            char line[256];
            if (S_ISDIR(file_stat.st_mode)) {
                snprintf(line, sizeof(line), "drwxr-xr-x 1 user user %8ld %s %s\r\n",
                        file_stat.st_size, time_str, entry->d_name);
            } else {
                snprintf(line, sizeof(line), "-rw-r--r-- 1 user user %8ld %s %s\r\n",
                        file_stat.st_size, time_str, entry->d_name);
            }
            strcat(buffer, line);
        }
    }
    closedir(dir);
}

long get_file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) {
        return st.st_size;
    }
    return -1;
}

int get_local_ip(char *ip_buffer, size_t buffer_size) {
    struct ifaddrs *ifaddr, *ifa;
    int found = 0;
    
    if (getifaddrs(&ifaddr) == -1) {
        return -1;
    }
    
    // Look for the first non-loopback IPv4 address
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL) continue;
        
        if (ifa->ifa_addr->sa_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)ifa->ifa_addr;
            // Skip loopback (127.0.0.1)
            if (sin->sin_addr.s_addr != htonl(INADDR_LOOPBACK)) {
                const char *ip = inet_ntoa(sin->sin_addr);
                if (ip) {
                    strncpy(ip_buffer, ip, buffer_size - 1);
                    ip_buffer[buffer_size - 1] = '\0';
                    found = 1;
                    break;
                }
            }
        }
    }
    
    freeifaddrs(ifaddr);
    
    if (!found) {
        // Fallback to localhost if no network interface found
        strncpy(ip_buffer, "127.0.0.1", buffer_size - 1);
        ip_buffer[buffer_size - 1] = '\0';
    }
    
    return found ? 0 : -1;
}

