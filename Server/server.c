/*
 * Server Application
 *
 * The server creates a TCP/IPv4 socket, binds it to an IP address and port, and listens for connections.
 * It accepts client connections, receives their names, and notifies all clients of new connections.
 * Each client is handled by a separate thread, which relays messages to all other clients and manages disconnections.
 */


#include "util.h"
#include <stdbool.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <arpa/inet.h>

// Structure to store accepted socket details including client name
struct AcceptedSocket {
    int socketFD;
    struct sockaddr_in address;
    bool success;
    char name[100]; // Field for the client's name
};

struct AcceptedSocket acceptedSockets[10];
int acceptedSocketsCount = 0;
pthread_mutex_t socketsMutex = PTHREAD_MUTEX_INITIALIZER;

// Function to send a message to all clients except the sender
void sendMsgToOthers(const char* message, struct AcceptedSocket* acceptedSocket) {
    pthread_mutex_lock(&socketsMutex);
    for (int i = 0; i < acceptedSocketsCount; i++) {
        if (acceptedSockets[i].socketFD != acceptedSocket->socketFD) {
            send(acceptedSockets[i].socketFD, message, strlen(message), 0);
        }
    }
    pthread_mutex_unlock(&socketsMutex);
}

// Function to format and send a message
void formatAndSend(const char* format, const char* name, const char* message, struct AcceptedSocket* acceptedSocket) {
    int size = snprintf(NULL, 0, format, name, message) + 1;
    char* buffer = malloc(size);
    if (buffer != NULL) {
        snprintf(buffer, size, format, name, message);
        printf("%s", buffer);
        sendMsgToOthers(buffer, acceptedSocket);
        free(buffer);
    }
}

// Function to receive data from a client and broadcast it to others
void* receiveData(void* acceptedSocket_ptr) {
    struct AcceptedSocket* acceptedSocket = (struct AcceptedSocket*)acceptedSocket_ptr;
    int clientSocketFD = acceptedSocket->socketFD;
    char buffer[1024];
    while (true) {
        ssize_t amountReceived = recv(clientSocketFD, buffer, sizeof(buffer) - 1, 0);
        if (amountReceived > 0) {
            buffer[amountReceived] = '\0';
            formatAndSend("%s: %s\n", acceptedSocket->name, buffer, acceptedSocket);
        } else if (amountReceived == 0) {
            formatAndSend("%s has disconnected.\n", acceptedSocket->name, "", acceptedSocket);
            break;
        } else {
            perror("recv failed");
            break;
        }
    }
    close(clientSocketFD);

    // Remove the client from the acceptedSockets array
    pthread_mutex_lock(&socketsMutex);
    for (int i = 0; i < acceptedSocketsCount; i++) {
        if (acceptedSockets[i].socketFD == clientSocketFD) {
            acceptedSockets[i] = acceptedSockets[--acceptedSocketsCount];
            break;
        }
    }
    pthread_mutex_unlock(&socketsMutex);
    free(acceptedSocket); // Free the allocated memory for acceptedSocket
    return NULL;
}

// Function to accept a new client connection and receive their name
struct AcceptedSocket* acceptConnection(int serverSocketFD) {
    struct sockaddr_in clientAddress;
    socklen_t clientAddressSize = sizeof(struct sockaddr_in);
    int clientSocketFD = accept(serverSocketFD, (struct sockaddr*)&clientAddress, &clientAddressSize);

    struct AcceptedSocket* acceptedSocket = malloc(sizeof(struct AcceptedSocket));
    if (acceptedSocket == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }

    acceptedSocket->address = clientAddress;
    acceptedSocket->socketFD = clientSocketFD;
    acceptedSocket->success = clientSocketFD >= 0;

    // Receive the client's name
    ssize_t nameLength = recv(clientSocketFD, acceptedSocket->name, sizeof(acceptedSocket->name) - 1, 0);
    if (nameLength > 0) {
        acceptedSocket->name[nameLength] = '\0';
        formatAndSend("%s has connected\n", acceptedSocket->name, "", acceptedSocket);
    } else {
        strcpy(acceptedSocket->name, "Unknown");
        printf("Failed receiving name.\n");
    }
    
    return acceptedSocket;
}

// Function to create a new thread to handle the client's communication
void createClientThread(struct AcceptedSocket *pSocket) {
    pthread_t id;
    if (pthread_create(&id, NULL, receiveData, pSocket) != 0) {
        perror("Failed to create thread");
        close(pSocket->socketFD);
        free(pSocket);
    } else {
        pthread_detach(id); // Detach the thread so it cleans up automatically when done
    }
}

// Function to continuously accept new client connections
void startAcceptingConnections(int serverSocketFD) {
    while (true) {
        struct AcceptedSocket* clientSocket = acceptConnection(serverSocketFD);
        if (clientSocket->success) {
            // Add the new client to the acceptedSockets array
            pthread_mutex_lock(&socketsMutex);
            acceptedSockets[acceptedSocketsCount++] = *clientSocket;
            pthread_mutex_unlock(&socketsMutex);
            createClientThread(clientSocket); // Create a new thread for the client
        } else {
            perror("Failed to accept connection");
            free(clientSocket);
        }
    }
}

int main() {
    int serverSocketFD = createTCPIpv4Socket();
    struct sockaddr_in* serverAddress = createIPv4Address("", 2000);

    int result = bind(serverSocketFD, (struct sockaddr*)serverAddress, sizeof(*serverAddress));
    if (result == 0)
        printf("Socket was bound successfully\n");
    else {
        perror("Bind failed");
        return 1;
    }

    if (listen(serverSocketFD, 10) != 0) {
        perror("Listen failed");
        return 1;
    }

    startAcceptingConnections(serverSocketFD); // Start accepting client connections

    shutdown(serverSocketFD, SHUT_RDWR);
    close(serverSocketFD);

    return 0;
}
