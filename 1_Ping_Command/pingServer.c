#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <pthread.h>

#define CLIENT_DETAILS_LOG_FILE "ping.log"
#define MESSAGE_SIZE 1024

pthread_mutex_t mutex;

// Function to write data to the log file
void writeDataToLogFile(char *ipAddress, int portNumber, double roundTripTime) {
    FILE *fp = fopen(CLIENT_DETAILS_LOG_FILE, "a");
    if (fp != NULL) {
        fprintf(fp, "Source IP address: %s, Port number: %d, RTT: %.2f ms\n", ipAddress, portNumber, roundTripTime);
        fclose(fp);
    } else {
        printf("Error opening log file.\n");
    }
}

// Function to send acknowledgement and return round trip time
double sendAcknowledgementAndGetRoundTripTime(int clientSocket, char *buffer) {
    struct timeval start, end;
    double roundTripTime;

    // Storing start time for round trip time
    gettimeofday(&start, NULL);

    pthread_mutex_lock(&mutex);
    // Acknowledging the received message
    send(clientSocket, buffer, strlen(buffer), 0);
    // receive acknowledgement
    recv(clientSocket, buffer, sizeof(buffer), 0);
    pthread_mutex_unlock(&mutex);

    gettimeofday(&end, NULL);
    // Calculating round-trip time (RTT)
    roundTripTime = (double)(end.tv_usec - start.tv_usec) / 1000;

    return roundTripTime;
}

void *receivePingFromClient(void *args) {
    int clientSocket = *(int *)args;
    struct sockaddr_in clientAddress;
    socklen_t addressSize = sizeof(clientAddress);
    char buffer[MESSAGE_SIZE];
    struct timeval start, end;
    double roundTripTime;

    getpeername(clientSocket, (struct sockaddr *)&clientAddress, &addressSize);

    while (1) {
        bzero(buffer, MESSAGE_SIZE);
        int recv_status = recv(clientSocket, buffer, sizeof(buffer), 0);

        // As recv_status has non-positive value, assume client is disconnected`
        if (recv_status <= 0) {
            printf("Client disconnected.\n");
            break;
        }

        roundTripTime = sendAcknowledgementAndGetRoundTripTime(clientSocket, buffer);

        pthread_mutex_lock(&mutex);
        writeDataToLogFile(inet_ntoa(clientAddress.sin_addr), ntohs(clientAddress.sin_port), roundTripTime);
        pthread_mutex_unlock(&mutex);
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
void assignAddressToServerSocket(int serverSocket, struct sockaddr_in *serverAddress, char *ipOfServer, int portOfServer) {
    memset(serverAddress, '\0', sizeof(*serverAddress));
    serverAddress->sin_family = AF_INET;
    serverAddress->sin_port = htons(portOfServer);
    serverAddress->sin_addr.s_addr = inet_addr(ipOfServer);
}

// Function to bind the server socket to the specified IP and port
void bindServerSocket(int serverSocket, struct sockaddr_in *serverAddress, int portOfServer) {
    if (bind(serverSocket, (struct sockaddr *)serverAddress, sizeof(*serverAddress)) < 0) {
        perror("Error in binding socket to address");
        exit(1);
    }
    printf("Server socket binded to the port number: %d\n", portOfServer);
}

// Function to listen for incoming connections
void listenForIncomingConnections(int serverSocket) {
    listen(serverSocket, 5);
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
}

void main(int argc, char *argv[]) {

    if (argc < 3) {
        printf("Usage: %s <server_ip> <port>\n", argv[0]);
        exit(1);
    }

    char *ipOfServer = argv[1];
    int port = atoi(argv[2]);

    int serverSocket;
    struct sockaddr_in serverAddress;

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
