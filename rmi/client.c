#include "rmi.h"

double invoke_remote_method(int sock, MethodID method_id, double arg1, double arg2) {
    RMIRequest req;
    RMIResponse resp;
    
    req.method_id = method_id;
    req.arg1 = arg1;
    req.arg2 = arg2;
    
    int sent = send(sock, &req, sizeof(RMIRequest), 0);
    if (sent != sizeof(RMIRequest)) {
        fprintf(stderr, "Error sending request to server\n");
        return 0.0;
    }

    int valread = recv(sock, &resp, sizeof(RMIResponse), 0);
    if (valread != sizeof(RMIResponse)) {
        fprintf(stderr, "Error receiving response from server\n");
        return 0.0;
    }
    
    if (!resp.success) {
        fprintf(stderr, "Remote method error: %s\n", resp.error_msg);
        return 0.0;
    }
    
    return resp.result;
}

int main(int argc, char *argv[]) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "192.168.1.79", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }
    
    printf("Connected to RMI server\n");
    
    if (argc == 4) {
        MethodID method = atoi(argv[1]);
        double arg1 = atof(argv[2]);
        double arg2 = atof(argv[3]);
        
        double result = invoke_remote_method(sock, method, arg1, arg2);
        printf("Result: %.2f\n", result);
    } else {
        printf("\nRMI Client - Available Methods:\n");
        printf("1. Add\n");
        printf("2. Subtract\n");
        printf("3. Multiply\n");
        printf("4. Divide\n");
        printf("99. Exit\n\n");
        
        int method_id;
        double arg1, arg2, result;
        
        while (1) {
            printf("Enter method ID (1-4, 99 to exit): ");
            scanf("%d", &method_id);
            
            if (method_id == METHOD_EXIT) {
                invoke_remote_method(sock, METHOD_EXIT, 0, 0);
                break;
            }
            
            if (method_id < 1 || method_id > 4) {
                printf("Invalid method ID\n");
                continue;
            }
            
            printf("Enter first argument: ");
            scanf("%lf", &arg1);
            printf("Enter second argument: ");
            scanf("%lf", &arg2);
            
            result = invoke_remote_method(sock, method_id, arg1, arg2);
            printf("Result: %.2f\n\n", result);
        }
    }
    
    close(sock);
    printf("Client disconnected\n");
    
    return 0;
}

