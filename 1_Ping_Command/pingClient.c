#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>

#define MESSAGE_SIZE 1024

#define INTEGER_ONE_THOUSAND 1000

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
    printf("Disconnected from the server.\n");
}

// Function to wait till the next ping
void waitTillNextPing(int current, int totalpingRequest, int timeGap) {
    if (current < totalpingRequest - 1) {
            sleep(timeGap);
    }
}

// Function to send ping requests to the server
void pingTheServer(int sock, int totalpingRequest, int timeGap) {
    char buffer[MESSAGE_SIZE];
    int i;
    struct timeval start, end;
    double rtt;

    for (i = 0; i < totalpingRequest; i++) {
        bzero(buffer, 1024);
        sprintf(buffer, "PING %d", i + 1);
        printf("Client: %s\n", buffer);

        // Assigning start with the current time before sending the ping
        gettimeofday(&start, NULL);

        send(sock, buffer, strlen(buffer), 0);

        bzero(buffer, MESSAGE_SIZE);
        recv(sock, buffer, sizeof(buffer), 0);

        // Assigning end with the current time after receiving the acknowledgment
        gettimeofday(&end, NULL);

        printf("Server: %s\n", buffer);

        // Calculate round-trip time by converting microseconds to milliseconds
        rtt = (double)(end.tv_usec - start.tv_usec) / INTEGER_ONE_THOUSAND;

        printf("RTT: %.2f ms\n", rtt);

        // Sending acknowledgment to the server
        send(sock, buffer, strlen(buffer), 0);

        // Waiting for the specified time gap before sending the next ping
        waitTillNextPing(i, totalpingRequest, timeGap);
    }
}

void main(int argc, char *argv[]) {
    if (argc < 5) {
        printf("Usage: %s <server_ip> <ping_count> <interval> <port>\n", argv[0]);
        exit(1);
    }

    char *ipOfServer = argv[1];
    int pingFreq = atoi(argv[2]);
    int timeGap = atoi(argv[3]);
    int serverPort = atoi(argv[4]);

    int socketC;
    struct sockaddr_in address;

    // Create TCP socket
    socketC = createSocket();

    // Assign address to socket
    assignAddressToSocket(socketC, &address, ipOfServer, serverPort);

    // Connect to the server
    connectToServer(socketC, &address);

    // Send ping requests to the server
    pingTheServer(socketC, pingFreq, timeGap);

    // Close the socket
    closeSocket(socketC);
}
