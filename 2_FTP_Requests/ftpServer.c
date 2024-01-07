#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MESSAGE_SIZE 1024

#define SERVER_BACKLOG 5

#define MESSAGE_END_TRANSFER "MESSAGE_END_TRANSFER"

#define ACK_MESSAGE "SUCCESS"
#define ACK_CLOSE_MESSAGE "ACK_CLOSE"
#define ACK_FILE_ERROR_MESSAGE "ACK_FILE_ERROR"
#define ACK_DIRECTORY_ERROR_MESSAGE "ACK_DIRECTORY_ERROR"
#define ACK_TRANSFER_BEGIN_MESSAGE "ACK_TRANSFER_BEGIN"
#define ACK_LS_ERROR_MESSAGE "ACK_LS_ERROR"

pthread_mutex_t mutex;

// Function to send acknowledgement to the client
void sendAcknowledgementToClient(int clientSocket, char *ackMessage) {
    char buffer[MESSAGE_SIZE];
    bzero(buffer, MESSAGE_SIZE);
    strcpy(buffer, ackMessage);
    send(clientSocket, buffer, strlen(buffer), 0);
}

// Function to remove leading and trailing spaces and return the string
char *trimString(char *string) {
    int i = 0;
    while (string[i] == ' ') {
        i++;
    }
    string = string + i;
    i = strlen(string) - 1;
    while (string[i] == ' ') {
        i--;
    }
    string[i + 1] = '\0';
    return string;
}

// Function to handle each client connection
void *receivePingFromClient(void *args) {
    int clientSocket = *(int *)args, receiveStatus;
    struct sockaddr_in clientAddress;
    socklen_t addressSize = sizeof(clientAddress);
    char buffer[MESSAGE_SIZE];

    while (1) {
        bzero(buffer, MESSAGE_SIZE);
        receiveStatus = recv(clientSocket, buffer, sizeof(buffer), 0);

        if (receiveStatus <= 0) {
            continue;
        }

        // If close string is received, close the connection
        if (strcmp(buffer, "close") == 0) {
            printf("Client disconnected.\n");
            sendAcknowledgementToClient(clientSocket, ACK_CLOSE_MESSAGE);
            break;
        }
        else if (strncmp(buffer, "put", 3) == 0) {
            // Copy filename from buffer
            char fileName[MESSAGE_SIZE];
            int errorFlag = 0;
            strcpy(fileName, buffer + 4);

            // Remove leading and trailing spaces
            trimString(fileName);

            // Making sure the file gets stored in folder 'Server'. Adding leading Server/ to the file name
            char temp[MESSAGE_SIZE];
            strcpy(temp, "Server/");
            strcat(temp, fileName);
            strcpy(fileName, temp);

            printf("File name in server: %s\n", fileName);

            pthread_mutex_lock(&mutex);
            FILE *fp = fopen(fileName, "w");
            if (fp == NULL) {
                printf("Error opening file.\n");
                sendAcknowledgementToClient(clientSocket, ACK_FILE_ERROR_MESSAGE);
            }
            else {
                
                // Send acknowledgement to client
                send(clientSocket, ACK_MESSAGE, strlen(ACK_MESSAGE), 0);

                while (1) {
                    bzero(buffer, MESSAGE_SIZE);
                    int recvStatus = recv(clientSocket, buffer, sizeof(buffer), 0);

                    // Checking if client has successfully opened the file
                    if(strcmp(buffer, ACK_FILE_ERROR_MESSAGE) == 0) {
                        printf("Error opening file in client side.\n");
                        remove(fileName);
                        errorFlag = 1;
                        break;
                    }
                    // Checking whether file transfer is complete
                    else if (strcmp(buffer, MESSAGE_END_TRANSFER) == 0) {
                        printf("File received.\n");
                        break;
                    }
                    
                    if (recvStatus <= 0){
                        break;
                    }
                    fprintf(fp, "%s", buffer);
                    bzero(buffer, MESSAGE_SIZE);
                    sendAcknowledgementToClient(clientSocket, ACK_MESSAGE);
                }
                fclose(fp);
                if(errorFlag == 0) {
                    sendAcknowledgementToClient(clientSocket, ACK_MESSAGE);
                }
            }
            pthread_mutex_unlock(&mutex);
        }
        else if (strncmp(buffer, "get", 3) == 0) {
            // Copy filename from buffer
            char fileName[MESSAGE_SIZE];
            strcpy(fileName, buffer + 4);

            // Remove leading and trailing spaces
            trimString(fileName);

            // Making sure the file gets copied from folder 'Server'. Adding leading Server/ to the file name
            char temp[MESSAGE_SIZE];
            strcpy(temp, "Server/");
            strcat(temp, fileName);
            strcpy(fileName, temp);

            printf("File name in server: %s\n", fileName);

            // Read data from file and send to server
            pthread_mutex_lock(&mutex);
            FILE *fp = fopen(fileName, "r");
            if (fp == NULL) {
                printf("Error opening file.\n");
                send(clientSocket, ACK_FILE_ERROR_MESSAGE, strlen(ACK_FILE_ERROR_MESSAGE), 0);
                pthread_mutex_unlock(&mutex);
                continue;
            } else {
                send(clientSocket, ACK_MESSAGE, strlen(ACK_MESSAGE), 0);
                bzero(buffer, MESSAGE_SIZE);
                recv(clientSocket, buffer, sizeof(buffer), 0);

                // Checking if client has successfully opened the file
                if(strcmp(buffer, ACK_FILE_ERROR_MESSAGE) == 0) {
                    printf("Error opening file in client side.\n");
                    fclose(fp);
                    pthread_mutex_unlock(&mutex);
                    continue;
                }
                

                // Read the data from file and send to client
                bzero(buffer, MESSAGE_SIZE);
                while(fgets(buffer, MESSAGE_SIZE, fp) != NULL) {
                    send(clientSocket, buffer, sizeof(buffer), 0);
                    bzero(buffer, MESSAGE_SIZE);
                    recv(clientSocket, buffer, sizeof(buffer), 0);
                    bzero(buffer, MESSAGE_SIZE);
                }
                fclose(fp);

                // Sending "MESSAGE_END_TRANSFER" to client to indicate end of file
                send(clientSocket, MESSAGE_END_TRANSFER, strlen(MESSAGE_END_TRANSFER), 0);
            }
            pthread_mutex_unlock(&mutex);

            // Receive acknowledgement from client
            bzero(buffer, MESSAGE_SIZE);
            recv(clientSocket, buffer, sizeof(buffer), 0);

            if(strcmp(buffer, ACK_MESSAGE) == 0) {
                printf("Transfer done.\n");
            }
        }
        else if (strncmp(buffer, "cd", 2) == 0) {
            // Copy directory name from buffer
            char directoryName[MESSAGE_SIZE];
            strcpy(directoryName, buffer + 3);

            // Remove leading and trailing spaces
            trimString(directoryName);

            // Change directory
            if (chdir(directoryName) == 0) {
                // Get current directory
                char currentDirectory[MESSAGE_SIZE];
                getcwd(currentDirectory, sizeof(currentDirectory));
                printf("Directory changed to %s\n", currentDirectory);
                sendAcknowledgementToClient(clientSocket, ACK_MESSAGE);
            }
            else {
                printf("Error changing directory.\n");
                sendAcknowledgementToClient(clientSocket, ACK_FILE_ERROR_MESSAGE);
            }
        }
        else if (strncmp(buffer, "pwd", 3) == 0) {
            // Get current directory
            char currentDirectory[MESSAGE_SIZE];
            getcwd(currentDirectory, sizeof(currentDirectory));
            printf("Current directory: %s\n", currentDirectory);
            sendAcknowledgementToClient(clientSocket, ACK_MESSAGE);
        }
        else if(strncmp(buffer, "ls", 2) == 0) {
            // Open pipe to execute ls command
            FILE *fp = popen("ls", "r");
            if (fp == NULL) {
                perror("Error opening pipe");
                sendAcknowledgementToClient(clientSocket, ACK_DIRECTORY_ERROR_MESSAGE);
                continue;
            }

            bzero(buffer, MESSAGE_SIZE);

            // Read output of ls command using fgets and send to client
            while(fgets(buffer, MESSAGE_SIZE, fp) != NULL) {
                send(clientSocket, buffer, sizeof(buffer), 0);
                bzero(buffer, MESSAGE_SIZE);
            }
            fclose(fp);

            // Sending "MESSAGE_END_TRANSFER" to client to indicate end of file
            send(clientSocket, MESSAGE_END_TRANSFER, strlen(MESSAGE_END_TRANSFER), 0);

            // Receive acknowledgement from client
            bzero(buffer, MESSAGE_SIZE);
            recv(clientSocket, buffer, sizeof(buffer), 0);

            if(strcmp(buffer, ACK_MESSAGE) == 0) {
                printf("Transfer for ls done.\n");
            }
            
        }
        else{
            printf("Message from client: %s\n", buffer);
            sendAcknowledgementToClient(clientSocket, ACK_MESSAGE);
        }

    }

    close(clientSocket);
    free(args);
    return NULL;
}

// Function to create the server socket
int createServerSocket() {
    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        perror("Error in creating socket");
        exit(1);
    }
    printf("TCP server socket created.\n");

    return serverSocket;
}

// Funtion to assign address to server
void assignAddressToServerSocket(int serverSocket, struct sockaddr_in *serverAddress, char *ipOfServer, int port) {
    memset(serverAddress, '\0', sizeof(*serverAddress));
    serverAddress->sin_family = AF_INET;
    serverAddress->sin_port = htons(port);
    serverAddress->sin_addr.s_addr = inet_addr(ipOfServer);
}

// Function to bind the server socket to the specified IP and port
void bindServerSocket(int serverSocket, struct sockaddr_in *serverAddress, int port) {
    if (bind(serverSocket, (struct sockaddr *)serverAddress, sizeof(*serverAddress)) < 0) {
        perror("Error in binding socket to address");
        exit(1);
    }
    printf("Server socket binded to the port number: %d\n", port);
}

// Function to listen for incoming connections
void listenForIncomingConnections(int serverSocket) {
    listen(serverSocket, SERVER_BACKLOG);
    printf("Listening...\n");
}

// Function to initialize the mutex
void initializeMutex() {
    if (pthread_mutex_init(&mutex, NULL) != 0) {
        perror("Error. Could not initialize mutex");
        exit(1);
    }
}

// Function to close the server socket and destroy the mutex
void closeServerSocketAndDestroyMutex(int serverSocket) {
    close(serverSocket);
    pthread_mutex_destroy(&mutex);
}

// Function to accept incoming connections
int acceptIncomingConnections(int serverSocket) {
    int clientSocket;
    struct sockaddr_in clientAddress;
    socklen_t addressSize = sizeof(clientAddress);

    clientSocket = accept(serverSocket, (struct sockaddr *)&clientAddress, &addressSize);
    if (clientSocket < 0) {
        perror("Error. Could not accept connection");
        exit(1);
    }
    printf("Client connected.\n");

    return clientSocket;
}


// Function to accept incoming connections and create a thread for each connection
void acceptIncomingConnectionsAndCreateThread(int serverSocket) {
    int clientSocket;

    while (1) {
        // Accepting incoming connections
        clientSocket = acceptIncomingConnections(serverSocket);

        pthread_t thread_id;
        int *newClientSocket = malloc(1);
        *newClientSocket = clientSocket;

        if (pthread_create(&thread_id, NULL, receivePingFromClient, (void *)newClientSocket) != 0) {
            perror("Error. Could not create thread");
            exit(1);
        }
        pthread_detach(thread_id);
    }

    pthread_exit(NULL);
}

void main(int argc, char *argv[]) {

    int serverSocket, port;
    struct sockaddr_in serverAddress;

    // Reading port from command line arguments
    if (argc != 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        exit(1);
    }
    
    char *ipOfServer = argv[1];
    port = atoi(argv[2]);

    // Initializing the mutex
    initializeMutex();

    // Creating the server socket
    serverSocket = createServerSocket();

    // Assigning address to the server socket
    assignAddressToServerSocket(serverSocket, &serverAddress, ipOfServer, port);

    // Binding the server socket to the specified IP and port
    bindServerSocket(serverSocket, &serverAddress, port);

    // Listening for incoming connections
    listenForIncomingConnections(serverSocket);

    // Acccepting incoming connections and creating a thread for each connection
    acceptIncomingConnectionsAndCreateThread(serverSocket);

    // Closing the server socket and destroying the mutex
    closeServerSocketAndDestroyMutex(serverSocket);
}
