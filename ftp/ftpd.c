#include "ftp_common.h"

static void handle_client(void *arg) {
    ftp_session_t *session = (ftp_session_t *)arg;
    char buffer[BUFFER_SIZE];
    char command[256];
    char argument[256];
    int n;
    
    getcwd(session->current_dir, sizeof(session->current_dir));
    session->authenticated = 0;
    session->data_fd = -1;
    session->passive_fd = -1;
    
    send_response(session->control_fd, FTP_READY, "FTP Server Ready");
    
    while (1) {
        n = recv(session->control_fd, buffer, sizeof(buffer) - 1, 0);
        if (n <= 0) break;
        
        buffer[n] = '\0';
        // Remove \r\n
        char *p = strchr(buffer, '\r');
        if (p) *p = '\0';
        p = strchr(buffer, '\n');
        if (p) *p = '\0';
        
        // Parse command
        sscanf(buffer, "%s %[^\r\n]", command, argument);
        
        if (strcmp(command, CMD_USER) == 0) {
            strncpy(session->username, argument, sizeof(session->username) - 1);
            send_response(session->control_fd, FTP_USERNAME_OK, "Password required");
        }
        else if (strcmp(command, CMD_PASS) == 0) {
            // Simple authentication - accept any password
            session->authenticated = 1;
            send_response(session->control_fd, FTP_LOGIN_SUCCESS, "Login successful");
        }
        else if (strcmp(command, CMD_QUIT) == 0) {
            send_response(session->control_fd, FTP_GOODBYE, "Goodbye");
            break;
        }
        else if (strcmp(command, CMD_PWD) == 0) {
            char response[512];
            snprintf(response, sizeof(response), "\"%s\"", session->current_dir);
            send_response(session->control_fd, FTP_PATHNAME_CREATED, response);
        }
        else if (strcmp(command, CMD_CWD) == 0) {
            if (chdir(argument) == 0) {
                getcwd(session->current_dir, sizeof(session->current_dir));
                send_response(session->control_fd, FTP_FILE_ACTION_OK, "Directory changed");
            } else {
                send_response(session->control_fd, FTP_FILE_NOT_FOUND, "Directory not found");
            }
        }
        else if (strcmp(command, CMD_PASV) == 0) {
            // Create passive mode data socket
            int pasv_sock = socket(AF_INET, SOCK_STREAM, 0);
            struct sockaddr_in pasv_addr;
            memset(&pasv_addr, 0, sizeof(pasv_addr));
            pasv_addr.sin_family = AF_INET;
            pasv_addr.sin_addr.s_addr = INADDR_ANY;
            pasv_addr.sin_port = 0; // Let system choose port
            
            if (bind(pasv_sock, (struct sockaddr *)&pasv_addr, sizeof(pasv_addr)) == 0 &&
                listen(pasv_sock, 1) == 0) {
                socklen_t len = sizeof(pasv_addr);
                getsockname(pasv_sock, (struct sockaddr *)&pasv_addr, &len);
                session->data_port = ntohs(pasv_addr.sin_port);
                session->passive_fd = pasv_sock;
                
                // Get local network IP address
                char local_ip[16];
                if (get_local_ip(local_ip, sizeof(local_ip)) == 0) {
                    // Parse IP address into comma-separated format
                    int h1, h2, h3, h4;
                    if (sscanf(local_ip, "%d.%d.%d.%d", &h1, &h2, &h3, &h4) == 4) {
                        char response[256];
                        snprintf(response, sizeof(response), 
                                "Entering Passive Mode (%d,%d,%d,%d,%d,%d)",
                                h1, h2, h3, h4,
                                session->data_port / 256, session->data_port % 256);
                        send_response(session->control_fd, FTP_PASSIVE_MODE, response);
                    } else {
                        // Fallback to localhost format
                        char response[256];
                        snprintf(response, sizeof(response), 
                                "Entering Passive Mode (127,0,0,1,%d,%d)",
                                session->data_port / 256, session->data_port % 256);
                        send_response(session->control_fd, FTP_PASSIVE_MODE, response);
                    }
                } else {
                    // Fallback to localhost
                    char response[256];
                    snprintf(response, sizeof(response), 
                            "Entering Passive Mode (127,0,0,1,%d,%d)",
                            session->data_port / 256, session->data_port % 256);
                    send_response(session->control_fd, FTP_PASSIVE_MODE, response);
                }
            } else {
                send_response(session->control_fd, FTP_ACTION_FAILED, "Passive mode failed");
            }
        }
        else if (strcmp(command, CMD_LIST) == 0) {
            if (!session->authenticated) {
                send_response(session->control_fd, FTP_NOT_LOGGED_IN, "Not logged in");
                continue;
            }
            
            // Accept data connection
            if (session->passive_fd >= 0) {
                session->data_fd = accept(session->passive_fd, NULL, NULL);
                close(session->passive_fd);
                session->passive_fd = -1;
            }
            
            if (session->data_fd >= 0) {
                send_response(session->control_fd, FTP_DATA_CONNECTION_OPEN, "Opening data connection");
                
                char file_list[BUFFER_SIZE * 4];
                get_file_list(session->current_dir, file_list, sizeof(file_list));
                send(session->data_fd, file_list, strlen(file_list), 0);
                close(session->data_fd);
                session->data_fd = -1;
                
                send_response(session->control_fd, FTP_CLOSING_DATA, "Transfer complete");
            } else {
                send_response(session->control_fd, FTP_ACTION_FAILED, "Data connection failed");
            }
        }
        else if (strcmp(command, CMD_RETR) == 0) {
            if (!session->authenticated) {
                send_response(session->control_fd, FTP_NOT_LOGGED_IN, "Not logged in");
                continue;
            }
            
            char file_path[MAX_PATH_LEN];
            snprintf(file_path, sizeof(file_path), "%s/%s", session->current_dir, argument);
            
            FILE *file = fopen(file_path, "rb");
            if (!file) {
                send_response(session->control_fd, FTP_FILE_NOT_FOUND, "File not found");
                continue;
            }
            
            // Accept data connection
            if (session->passive_fd >= 0) {
                session->data_fd = accept(session->passive_fd, NULL, NULL);
                close(session->passive_fd);
                session->passive_fd = -1;
            }
            
            if (session->data_fd >= 0) {
                send_response(session->control_fd, FTP_DATA_CONNECTION_OPEN, "Opening data connection");
                
                char file_buffer[BUFFER_SIZE];
                size_t bytes_read;
                while ((bytes_read = fread(file_buffer, 1, sizeof(file_buffer), file)) > 0) {
                    send(session->data_fd, file_buffer, bytes_read, 0);
                }
                
                fclose(file);
                close(session->data_fd);
                session->data_fd = -1;
                send_response(session->control_fd, FTP_CLOSING_DATA, "Transfer complete");
            } else {
                fclose(file);
                send_response(session->control_fd, FTP_ACTION_FAILED, "Data connection failed");
            }
        }
        else if (strcmp(command, CMD_STOR) == 0) {
            if (!session->authenticated) {
                send_response(session->control_fd, FTP_NOT_LOGGED_IN, "Not logged in");
                continue;
            }
            
            char file_path[MAX_PATH_LEN];
            snprintf(file_path, sizeof(file_path), "%s/%s", session->current_dir, argument);
            
            // Accept data connection
            if (session->passive_fd >= 0) {
                session->data_fd = accept(session->passive_fd, NULL, NULL);
                close(session->passive_fd);
                session->passive_fd = -1;
            }
            
            if (session->data_fd >= 0) {
                send_response(session->control_fd, FTP_DATA_CONNECTION_OPEN, "Opening data connection");
                
                FILE *file = fopen(file_path, "wb");
                if (file) {
                    char file_buffer[BUFFER_SIZE];
                    int bytes_received;
                    while ((bytes_received = recv(session->data_fd, file_buffer, sizeof(file_buffer), 0)) > 0) {
                        fwrite(file_buffer, 1, bytes_received, file);
                    }
                    fclose(file);
                    send_response(session->control_fd, FTP_CLOSING_DATA, "Transfer complete");
                } else {
                    send_response(session->control_fd, FTP_ACTION_FAILED, "Cannot create file");
                }
                
                close(session->data_fd);
                session->data_fd = -1;
            } else {
                send_response(session->control_fd, FTP_ACTION_FAILED, "Data connection failed");
            }
        }
        else if (strcmp(command, CMD_TYPE) == 0) {
            send_response(session->control_fd, FTP_FILE_ACTION_OK, "Type set");
        }
        else if (strcmp(command, CMD_SYST) == 0) {
            send_response(session->control_fd, 215, "UNIX Type: L8");
        }
        else {
            send_response(session->control_fd, 502, "Command not implemented");
        }
    }
    
    if (session->data_fd >= 0) close(session->data_fd);
    if (session->passive_fd >= 0) close(session->passive_fd);
    close(session->control_fd);
    free(session);
    pthread_exit(NULL);
}

int ftpd_start(int port) {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    pthread_t thread;
    
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return -1;
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_fd);
        return -1;
    }
    
    if (listen(server_fd, 5) < 0) {
        perror("listen");
        close(server_fd);
        return -1;
    }
    
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        
        ftp_session_t *session = malloc(sizeof(ftp_session_t));
        session->control_fd = client_fd;
        
        pthread_create(&thread, NULL, (void *)handle_client, session);
        pthread_detach(thread);
    }
    
    close(server_fd);
    return 0;
}

