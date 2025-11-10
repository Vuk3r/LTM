#ifndef FTP_CLIENT_H
#define FTP_CLIENT_H

#include "ftp_common.h"

typedef struct {
    int control_fd;
    int data_fd;
    int passive_fd;
    char server_ip[64];
    int server_port;
    int authenticated;
} ftp_client_t;

ftp_client_t *ftp_client_new(void);
void ftp_client_free(ftp_client_t *client);
int ftp_client_connect(ftp_client_t *client, const char *ip, int port);
int ftp_client_login(ftp_client_t *client, const char *username, const char *password);
int ftp_client_list(ftp_client_t *client, char *buffer, size_t buffer_size);
int ftp_client_retr(ftp_client_t *client, const char *remote_file, const char *local_file);
int ftp_client_stor(ftp_client_t *client, const char *local_file, const char *remote_file);
int ftp_client_cwd(ftp_client_t *client, const char *dir);
int ftp_client_pwd(ftp_client_t *client, char *buffer, size_t size);
int ftp_client_quit(ftp_client_t *client);

#endif

