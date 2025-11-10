#ifndef FTP_COMMON_H
#define FTP_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <ifaddrs.h>
#include <netdb.h>

#define FTP_PORT 2121
#define DATA_PORT_BASE 5000
#define BUFFER_SIZE 4096
#define MAX_PATH_LEN 512

// FTP response codes
#define FTP_READY 220
#define FTP_GOODBYE 221
#define FTP_DATA_CONNECTION_OPEN 225
#define FTP_CLOSING_DATA 226
#define FTP_PASSIVE_MODE 227
#define FTP_LOGIN_SUCCESS 230
#define FTP_FILE_ACTION_OK 250
#define FTP_PATHNAME_CREATED 257
#define FTP_USERNAME_OK 331
#define FTP_NOT_LOGGED_IN 530
#define FTP_NEED_ACCOUNT 532
#define FTP_FILE_NOT_FOUND 550
#define FTP_ACTION_FAILED 550

// FTP commands
#define CMD_USER "USER"
#define CMD_PASS "PASS"
#define CMD_QUIT "QUIT"
#define CMD_PWD "PWD"
#define CMD_CWD "CWD"
#define CMD_LIST "LIST"
#define CMD_RETR "RETR"
#define CMD_STOR "STOR"
#define CMD_PASV "PASV"
#define CMD_PORT "PORT"
#define CMD_TYPE "TYPE"
#define CMD_MODE "MODE"
#define CMD_STRU "STRU"
#define CMD_SYST "SYST"
#define CMD_FEAT "FEAT"
#define CMD_SIZE "SIZE"
#define CMD_DELE "DELE"
#define CMD_MKD "MKD"
#define CMD_RMD "RMD"

typedef struct {
    int control_fd;
    int data_fd;
    int passive_fd;
    char current_dir[MAX_PATH_LEN];
    char username[64];
    int authenticated;
    int data_port;
    struct sockaddr_in data_addr;
} ftp_session_t;

// Utility functions
void send_response(int fd, int code, const char *message);
int parse_port_command(const char *cmd, struct sockaddr_in *addr);
void get_file_list(const char *path, char *buffer, size_t buffer_size);
long get_file_size(const char *path);
int get_local_ip(char *ip_buffer, size_t buffer_size);

#endif

