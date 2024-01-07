#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MESSAGE_SIZE 1024

#define MESSAGE_END_TRANSFER "MESSAGE_END_TRANSFER"

#define ACK_MESSAGE "SUCCESS"
#define ACK_CLOSE_MESSAGE "ACK_CLOSE"
#define ACK_FILE_ERROR_MESSAGE "ACK_FILE_ERROR"
#define ACK_DIRECTORY_ERROR_MESSAGE "ACK_DIRECTORY_ERROR"
#define ACK_TRANSFER_BEGIN_MESSAGE "ACK_TRANSFER_BEGIN"
#define ACK_LS_ERROR_MESSAGE "ACK_LS_ERROR"

pthread_mutex_t mutex;

// Function to create TCP socket
int createSocket() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Error creating socket.");
        exit(1);
    }
    printf("TCP server socket created.\n");
    return sock;
}

// Function to assign address to socket
void assignAddressToSocket(int sock, struct sockaddr_in *address, char *ipOfServer, int portOfServer) {
    memset(address, '\0', sizeof(*address));
    address->sin_family = AF_INET;
    address->sin_port = htons(portOfServer);
    address->sin_addr.s_addr = inet_addr(ipOfServer);
}

// Function to connect to the server
void connectToServer(int sock, struct sockaddr_in *address) {
    if (connect(sock, (struct sockaddr*)address, sizeof(*address)) < 0) {
        perror("Not able to connect to server.");
        exit(1);
    }
    printf("Connected to the server.\n");
}

// Function to close the socket
void closeSocket(int sock) {
    close(sock);
    printf("Socket closed from client side.\n");
}

// Function to send acknowledgement to the client
void sendAcknowledgementToServer(int serverSocket, char *ackMessage) {
    char buffer[MESSAGE_SIZE];
    bzero(buffer, MESSAGE_SIZE);
    strcpy(buffer, ackMessage);
    send(serverSocket, buffer, strlen(buffer), 0);
}

// Function to read the command from the terminal and send to the server
void pingTheServer(int sock) {
    char buffer[MESSAGE_SIZE];

    while(1) {
        bzero(buffer, MESSAGE_SIZE);
        printf("ftp_client> ");
        fgets(buffer, MESSAGE_SIZE, stdin);

        // Remove trailing newline character
        buffer[strcspn(buffer, "\n")] = 0;

        // Check the message to be sent
        if(strcmp(buffer, "close") == 0) {
            send(sock, buffer, strlen(buffer), 0);

            // Receive acknowledgement from server
            bzero(buffer, MESSAGE_SIZE);
            recv(sock, buffer, sizeof(buffer), 0);

            if(strcmp(buffer, ACK_CLOSE_MESSAGE) == 0) {
                printf("Disconnected from server.\n");
                return;
            }
        } else if(strncmp(buffer, "put", 3) == 0) {
            // Send command to server
            send(sock, buffer, strlen(buffer), 0);

            // Extracting file name from the command. Making sure the file gets copied from folder 'Client'. Adding leading Client/ to the file name
            char fileName[MESSAGE_SIZE];
            strcpy(fileName, "Client/");
            strcat(fileName, buffer + 4);
            printf("File name in client: %s\n", fileName);

            // Receive acknowledgement from server
            bzero(buffer, MESSAGE_SIZE);
            recv(sock, buffer, sizeof(buffer), 0);

            // Checking if there is any error in opening file in server side
            if(strcmp(buffer, ACK_FILE_ERROR_MESSAGE) == 0) {
                printf("Error opening file in server side.\n");
                continue;
            }
            
            pthread_mutex_lock(&mutex);

            FILE *fp = fopen(fileName, "r");
            if (fp == NULL) {
                printf("Error opening file.\n");
                send(sock, ACK_FILE_ERROR_MESSAGE, strlen(ACK_FILE_ERROR_MESSAGE), 0);
                continue;
            } else {
                // Read the data from file and send to server
                bzero(buffer, MESSAGE_SIZE);
                while(fgets(buffer, MESSAGE_SIZE, fp) != NULL) {
                    send(sock, buffer, sizeof(buffer), 0);
                    bzero(buffer, MESSAGE_SIZE);
                    recv(sock, buffer, sizeof(buffer), 0);
                    bzero(buffer, MESSAGE_SIZE);
                }
                fclose(fp);

                // Sending "MESSAGE_END_TRANSFER" to client to indicate end of file
                send(sock, MESSAGE_END_TRANSFER, strlen(MESSAGE_END_TRANSFER), 0);
            }
            pthread_mutex_unlock(&mutex);

            // Receive acknowledgement from server
            bzero(buffer, MESSAGE_SIZE);
            recv(sock, buffer, sizeof(buffer), 0);

            if(strcmp(buffer, ACK_MESSAGE) == 0) {
                printf("Transfer done.\n");
                continue;
            }
        } else if(strncmp(buffer, "get", 3) == 0) {
            // Send command to server
            send(sock, buffer, strlen(buffer), 0);

            // Extracting file name from the command
            char fileName[MESSAGE_SIZE];
            strcpy(fileName, buffer + 4);

            // Making sure the file gets stored in folder 'Client'. Adding leading Client/ to the file name
            char temp[MESSAGE_SIZE];
            strcpy(temp, "Client/");
            strcat(temp, fileName);
            strcpy(fileName, temp);
            
            printf("File name in client: %s\n", fileName);

            // Receive acknowledgement from server
            bzero(buffer, MESSAGE_SIZE);
            recv(sock, buffer, sizeof(buffer), 0);

            // Checking if there is any error in opening file in server side
            if(strcmp(buffer, ACK_FILE_ERROR_MESSAGE) == 0) {
                printf("Error opening file in server side.\n");
                continue;
            }

            pthread_mutex_lock(&mutex);
            FILE *fp = fopen(fileName, "w");
            if (fp == NULL) {
                printf("Error opening file.\n");
                sendAcknowledgementToServer(sock, ACK_FILE_ERROR_MESSAGE);
            }
            else {
                sendAcknowledgementToServer(sock, ACK_MESSAGE);
                while (1) {
                    bzero(buffer, MESSAGE_SIZE);
                    int recvStatus = recv(sock, buffer, sizeof(buffer), 0);
                    
                    // Checking whether file transfer is complete
                    if (strcmp(buffer, MESSAGE_END_TRANSFER) == 0) {
                        printf("File received.\n");
                        break;
                    }
                    
                    if (recvStatus <= 0){
                        break;
                    }
                    fprintf(fp, "%s", buffer);
                    bzero(buffer, MESSAGE_SIZE);
                    sendAcknowledgementToServer(sock, ACK_MESSAGE);
                }
                fclose(fp);
                sendAcknowledgementToServer(sock, ACK_MESSAGE);
            }
            pthread_mutex_unlock(&mutex);
        } else if(strncmp(buffer, "cd", 2) == 0) {
            send(sock, buffer, strlen(buffer), 0);

            // Receive acknowledgement from server
            bzero(buffer, MESSAGE_SIZE);
            recv(sock, buffer, sizeof(buffer), 0);

            if(strcmp(buffer, ACK_MESSAGE) == 0) {
                printf("Directory changed.\n");
                continue;
            }
            else if(strcmp(buffer, ACK_DIRECTORY_ERROR_MESSAGE) == 0) {
                printf("Error changing directory.\n");
                continue;
            }
        } else if(strncmp(buffer, "pwd", 3) == 0) {
            // Send command to server
            send(sock, buffer, strlen(buffer), 0);

            // Receive acknowledgement from server
            bzero(buffer, MESSAGE_SIZE);
            recv(sock, buffer, sizeof(buffer), 0);

            if(strcmp(buffer, ACK_DIRECTORY_ERROR_MESSAGE) == 0) {
                printf("Error getting current directory.\n");
                continue;
            }
        } else if(strncmp(buffer, "ls", 2) == 0) {
            // Send command to server
            send(sock, buffer, strlen(buffer), 0);

            if(strcmp(buffer, ACK_LS_ERROR_MESSAGE) == 0) {
                printf("Error getting directory details.\n");
                sendAcknowledgementToServer(sock, ACK_LS_ERROR_MESSAGE);
                continue;
            }

            // Receive data from server and write the content to terminal
            bzero(buffer, MESSAGE_SIZE);
            while (1) {
                int recv_status = recv(sock, buffer, sizeof(buffer), 0);

                // As recv_status has non-positive value, assume server is disconnected`
                if (recv_status <= 0) {
                    printf("Server disconnected.\n");
                    break;
                }

                // Checking whether file transfer is complete
                if (strcmp(buffer, MESSAGE_END_TRANSFER) == 0) {
                    printf("Data received.\n");
                    break;
                }

                // Write buffer contents to terminal
                printf("%s", buffer);

                fflush(stdout);

                // Clear buffer
                bzero(buffer, MESSAGE_SIZE);
            }

            // Send acknowledgement to server
            sendAcknowledgementToServer(sock, ACK_MESSAGE);

            printf("Writing done\n");
        }
        else {
            printf("Incorrect command: %s\n", buffer);
        }
    }
}

void main(int argc, char *argv[]) {
    if (argc != 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        exit(1);
    }

    char *ipOfServer = argv[1];
    int serverPort = atoi(argv[2]);
    int socketC;
    struct sockaddr_in address;

    // initialize mutex
    pthread_mutex_init(&mutex, NULL);

    // Create TCP socket
    socketC = createSocket();

    // Assign address to socket
    assignAddressToSocket(socketC, &address, ipOfServer, serverPort);

    // Connect to the server
    connectToServer(socketC, &address);

    // Send ping requests to the server
    pingTheServer(socketC);

    // Close the socket
    closeSocket(socketC);

    // destroy mutex
    pthread_mutex_destroy(&mutex);
}
