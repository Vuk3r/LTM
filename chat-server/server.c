#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>

#define PORT 9001
#define MAX_CLIENTS 100
#define BUF_SIZE 1024

typedef struct
{
    int sockfd;
    int id;
} client_t;

client_t clients[MAX_CLIENTS];
int client_count = 0;
int next_id = 1;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void broadcast_message(char *message, int sender_id)
{
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].id != sender_id)
        {
            send(clients[i].sockfd, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void *handle_client(void *arg)
{ 
    client_t cli = *((client_t *)arg);
    char buffer[BUF_SIZE];
    int bytes_read;

    // gửi cho client biết số thứ tự của mình
    char welcome[BUF_SIZE];
    snprintf(welcome, sizeof(welcome), "You are Client %d\n", cli.id);
    send(cli.sockfd, welcome, strlen(welcome), 0);

    // thông báo cho các client khác
    char join_msg[BUF_SIZE];
    snprintf(join_msg, sizeof(join_msg), "[Server] Client %d has joined.\n", cli.id);
    broadcast_message(join_msg, cli.id);
    printf("%s", join_msg);

    while ((bytes_read = recv(cli.sockfd, buffer, BUF_SIZE, 0)) > 0)
    {
        buffer[bytes_read] = '\0';

        char msg[BUF_SIZE];
        snprintf(msg, sizeof(msg), "[Client %d] %s\n", cli.id, buffer);

        printf("%s", msg);
        broadcast_message(msg, cli.id);
    }

    // client thoát
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++)
    {
        if (clients[i].id == cli.id)
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
    snprintf(leave_msg, sizeof(leave_msg), "[Server] Client %d has left.\n", cli.id);
    broadcast_message(leave_msg, cli.id);
    printf("%s", leave_msg);

    close(cli.sockfd);
    free(arg);
    return NULL;
}

int main()
{
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1)
    {
        perror("Socket failed");
        exit(1);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    inet_pton(AF_INET, "172.28.210.246", &server_addr.sin_addr);

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

    printf("Chat server started on port %d...\n", PORT);

    while (1)
    {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_fd < 0)
        {
            perror("Accept failed");
            continue;
        }

        pthread_mutex_lock(&clients_mutex);
        client_t *cli = malloc(sizeof(client_t));
        cli->sockfd = client_fd;
        cli->id = next_id++;
        clients[client_count++] = *cli;
        pthread_mutex_unlock(&clients_mutex);

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, cli);
        pthread_detach(tid);
    }

    close(server_fd);
    return 0;
}
