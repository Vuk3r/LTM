#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define DEFAULT_PORT 9001
#define MAX_CLIENTS 100
#define BUF_SIZE 2048

typedef struct
{
    int sockfd;
    int id;
    char name[64];
} client_t;

client_t clients[MAX_CLIENTS];
int client_count = 0;
int next_id = 1;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Broadcast message to all clients except sender
void broadcast_message(char *message, int sender_id)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].id != sender_id)
        {
            ssize_t sent = send(clients[i].sockfd, message, strlen(message), 0);
            if (sent < 0) {
                perror("send failed in broadcast");
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Send unicast message to specific client
void unicast_message(char *message, int recipient_id, int sender_id)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].id == recipient_id)
        {
            ssize_t sent = send(clients[i].sockfd, message, strlen(message), 0);
            if (sent < 0) {
                perror("send failed in unicast");
            }
            pthread_mutex_unlock(&clients_mutex);
            return;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    // Recipient not found, notify sender
    char error_msg[BUF_SIZE];
    snprintf(error_msg, sizeof(error_msg), "[Server] Client %d not found.\n", recipient_id);
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].id == sender_id)
        {
            send(clients[i].sockfd, error_msg, strlen(error_msg), 0);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

// Send multicast message to multiple clients
void multicast_message(char *message, char *recipient_ids_str, int sender_id)
{
    char *ids_str = strdup(recipient_ids_str);
    char *token;
    int recipient_ids[MAX_CLIENTS];
    int recipient_count = 0;
    
    // Parse comma-separated IDs
    token = strtok(ids_str, ",");
    while (token != NULL && recipient_count < MAX_CLIENTS)
    {
        recipient_ids[recipient_count++] = atoi(token);
        token = strtok(NULL, ",");
    }
    
    pthread_mutex_lock(&clients_mutex);
    int found_count = 0;
    for (int i = 0; i < recipient_count; i++)
    {
        for (int j = 0; j < client_count; j++)
        {
            if (clients[j].id == recipient_ids[i] && clients[j].id != sender_id)
            {
                ssize_t sent = send(clients[j].sockfd, message, strlen(message), 0);
                if (sent < 0) {
                    perror("send failed in multicast");
                } else {
                    found_count++;
                }
                break;
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    
    free(ids_str);
    
    // Notify sender if some recipients not found
    if (found_count < recipient_count)
    {
        char error_msg[BUF_SIZE];
        snprintf(error_msg, sizeof(error_msg), 
                "[Server] Some recipients not found. Sent to %d/%d clients.\n", 
                found_count, recipient_count);
        for (int i = 0; i < client_count; i++)
        {
            if (clients[i].id == sender_id)
            {
                send(clients[i].sockfd, error_msg, strlen(error_msg), 0);
                break;
            }
        }
    }
}

// Get client by ID
client_t *get_client_by_id(int id)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].id == id)
        {
            pthread_mutex_unlock(&clients_mutex);
            return &clients[i];
        }
    }
    pthread_mutex_unlock(&clients_mutex);
    return NULL;
}

// Parse and handle message
void handle_message(char *buffer, int sender_id)
{
    // Make a copy since strtok modifies the buffer
    char buffer_copy[BUF_SIZE];
    strncpy(buffer_copy, buffer, BUF_SIZE - 1);
    buffer_copy[BUF_SIZE - 1] = '\0';
    
    char *type = strtok(buffer_copy, ":");
    if (type == NULL) return;
    
    char *message = NULL;
    char *recipients = NULL;
    
    if (strcmp(type, "BROADCAST") == 0)
    {
        // For BROADCAST::message format, find the message part after ::
        // Search in original buffer (before strtok modified the copy)
        char *double_colon = strstr(buffer, "::");
        if (double_colon != NULL)
        {
            message = double_colon + 2; // Skip the ::
            // Trim leading whitespace
            while (*message == ' ' || *message == '\t') message++;
        }
        else
        {
            // Fallback: try to get message after first colon
            recipients = strtok(NULL, ":");
            message = strtok(NULL, ":");
        }
        
        if (message == NULL || strlen(message) == 0) {
            printf("Warning: Empty BROADCAST message from client %d\n", sender_id);
            return;
        }
        
        // Broadcast to all except sender
        char formatted_msg[BUF_SIZE];
        snprintf(formatted_msg, sizeof(formatted_msg), "[Client %d - Broadcast]: %s\n", sender_id, message);
        printf("Server broadcasting: %s", formatted_msg);
        broadcast_message(formatted_msg, sender_id);
    }
    else if (strcmp(type, "UNICAST") == 0)
    {
        recipients = strtok(NULL, ":");
        message = strtok(NULL, ":");
        
        if (recipients == NULL || message == NULL) return;
        
        int recipient_id = atoi(recipients);
        
        char formatted_msg[BUF_SIZE];
        snprintf(formatted_msg, sizeof(formatted_msg), "[Client %d - Unicast to %d]: %s\n", 
                sender_id, recipient_id, message);
        printf("%s", formatted_msg);
        unicast_message(formatted_msg, recipient_id, sender_id);
    }
    else if (strcmp(type, "MULTICAST") == 0)
    {
        recipients = strtok(NULL, ":");
        message = strtok(NULL, ":");
        
        if (recipients == NULL || message == NULL) return;
        
        char formatted_msg[BUF_SIZE];
        snprintf(formatted_msg, sizeof(formatted_msg), "[Client %d - Multicast to %s]: %s\n", 
                sender_id, recipients, message);
        printf("%s", formatted_msg);
        multicast_message(formatted_msg, recipients, sender_id);
    }
}

void *handle_client(void *arg)
{ 
    client_t *cli = (client_t *)arg;
    char buffer[BUF_SIZE];
    int bytes_read;

    // Send client their ID
    char welcome[BUF_SIZE];
    snprintf(welcome, sizeof(welcome), "[Server] You are Client %d\n", cli->id);
    send(cli->sockfd, welcome, strlen(welcome), 0);

    // Notify other clients
    char join_msg[BUF_SIZE];
    snprintf(join_msg, sizeof(join_msg), "[Server] Client %d has joined.\n", cli->id);
    broadcast_message(join_msg, cli->id);
    printf("%s", join_msg);

    while ((bytes_read = recv(cli->sockfd, buffer, BUF_SIZE - 1, 0)) > 0)
    {
        buffer[bytes_read] = '\0';
        
        // Remove trailing newline and carriage return if present
        char *p = buffer + bytes_read - 1;
        while (p >= buffer && (*p == '\n' || *p == '\r')) {
            *p = '\0';
            p--;
        }
        
        // Process the message
        if (strlen(buffer) > 0) {
            handle_message(buffer, cli->id);
        }
    }

    // Client disconnected
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].id == cli->id)
        {
            for (int j = i; j < client_count - 1; j++)
            {
                clients[j] = clients[j + 1];
            }
            break;
        }
    }
    client_count--;
    pthread_mutex_unlock(&clients_mutex);

    char leave_msg[BUF_SIZE];
    snprintf(leave_msg, sizeof(leave_msg), "[Server] Client %d has left.\n", cli->id);
    broadcast_message(leave_msg, cli->id);
    printf("%s", leave_msg);

    close(cli->sockfd);
    free(cli);
    return NULL;
}

int main(int argc, char *argv[])
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    char *server_ip = NULL;
    int port = DEFAULT_PORT;

    // Parse command-line arguments
    if (argc < 3)
    {
        printf("Usage: %s <IP_ADDRESS> <PORT>\n", argv[0]);
        printf("Example: %s 0.0.0.0 9001\n", argv[0]);
        exit(1);
    }

    server_ip = argv[1];
    port = atoi(argv[2]);

    if (port <= 0 || port > 65535)
    {
        printf("Error: Invalid port number. Port must be between 1 and 65535.\n");
        exit(1);
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("Socket failed");
        exit(1);
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0)
    {
        printf("Error: Invalid IP address: %s\n", server_ip);
        close(server_fd);
        exit(1);
    }

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("Bind failed");
        close(server_fd);
        exit(1);
    }

    if (listen(server_fd, 10) < 0)
    {
        perror("Listen failed");
        close(server_fd);
        exit(1);
    }

    printf("Chat server started on %s:%d...\n", server_ip, port);
    printf("Waiting for connections...\n");

    while (1)
    {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_fd < 0)
        {
            perror("Accept failed");
            continue;
        }

        // Log client connection
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("Connection attempt from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        pthread_mutex_lock(&clients_mutex);
        if (client_count >= MAX_CLIENTS)
        {
            printf("Maximum clients reached. Rejecting connection from %s\n", client_ip);
            close(client_fd);
            pthread_mutex_unlock(&clients_mutex);
            continue;
        }
        
        client_t *cli = malloc(sizeof(client_t));
        cli->sockfd = client_fd;
        cli->id = next_id++;
        clients[client_count++] = *cli;
        pthread_mutex_unlock(&clients_mutex);

        printf("Client %d connected from %s\n", cli->id, client_ip);

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, cli);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
