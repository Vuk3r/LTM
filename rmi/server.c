#include "rmi.h"

double remote_add(double a, double b) {
    return a + b;
}

double remote_subtract(double a, double b) {
    return a - b;
}

double remote_multiply(double a, double b) {
    return a * b;
}

double remote_divide(double a, double b) {
    if (b == 0.0) {
        return 0.0;
    }
    return a / b;
}

void process_request(RMIRequest *req, RMIResponse *resp) {
    resp->success = 1;
    resp->result = 0.0;
    resp->error_msg[0] = '\0';
    
    switch (req->method_id) {
        case METHOD_ADD:
            resp->result = remote_add(req->arg1, req->arg2);
            break;
            
        case METHOD_SUBTRACT:
            resp->result = remote_subtract(req->arg1, req->arg2);
            break;
            
        case METHOD_MULTIPLY:
            resp->result = remote_multiply(req->arg1, req->arg2);
            break;
            
        case METHOD_DIVIDE:
            if (req->arg2 == 0.0) {
                resp->success = 0;
                strcpy(resp->error_msg, "Division by zero");
            } else {
                resp->result = remote_divide(req->arg1, req->arg2);
            }
            break;
            
        default:
            resp->success = 0;
            strcpy(resp->error_msg, "Unknown method ID");
            break;
    }
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }
    
    printf("RMI Server listening on port %d...\n", PORT);
    
    while (1) {
        if ((client_socket = accept(server_fd, (struct sockaddr *)&address, 
                                    (socklen_t*)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }
        
        printf("Client connected from %s:%d\n", 
               inet_ntoa(address.sin_addr), ntohs(address.sin_port));
        
        while (1) {
            RMIRequest req;
            RMIResponse resp;
            
            int valread = recv(client_socket, &req, sizeof(RMIRequest), 0);
            if (valread == sizeof(RMIRequest)) {
                printf("Received request: Method %d, Args: %.2f, %.2f\n", 
                       req.method_id, req.arg1, req.arg2);
                
                if (req.method_id == METHOD_EXIT) {
                    printf("Client requested disconnect\n");
                    close(client_socket);
                    break;
                }
                
                process_request(&req, &resp);
                
                send(client_socket, &resp, sizeof(RMIResponse), 0);
                
                printf("Sent response: Success=%d, Result=%.2f\n", 
                       resp.success, resp.result);
            } else if (valread <= 0) {
                printf("Client disconnected or connection closed\n");
                break;
            }
        }
        
        close(client_socket);
    }
    
    close(server_fd);
    printf("Server shutting down...\n");
    
    return 0;
}

