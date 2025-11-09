#ifndef RMI_H
#define RMI_H

#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 8080
#define MAX_BUFFER_SIZE 1024

typedef enum {
    METHOD_ADD = 1,
    METHOD_SUBTRACT = 2,
    METHOD_MULTIPLY = 3,
    METHOD_DIVIDE = 4,
    METHOD_EXIT = 99
} MethodID;

typedef struct {
    MethodID method_id;
    double arg1;
    double arg2;
} RMIRequest;

typedef struct {
    int success;
    double result;
    char error_msg[256];
} RMIResponse;


#endif

