/*
 * Client Application
 *
 * The client creates a TCP/IPv4 socket, connects to the server, and sends its name. 
 * It sends messages to the server, which broadcasts them to other clients, and receives messages from others to display. 
 * The client stays connected for message exchanges until it or server disconnects.
 */


#include <stdbool.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "util.h"

// Function executed by the thread to listen for incoming messages
void* startListening(void* socketFD_ptr) {
    int socketFD = *(int*)socketFD_ptr;
    free(socketFD_ptr); 
    char buffer[1024]; // Buffer to store incoming messages
    while (true) {
        ssize_t amountReceived = recv(socketFD, buffer, sizeof(buffer) - 1, 0); // Receive data from the socket
        if (amountReceived > 0) {
            buffer[amountReceived] = '\0'; // Null-terminate the received data
            printf("%s\n", buffer); 
        } else if (amountReceived == 0) {
            printf("Connection closed by server.\n");
            break;
        } else {
            perror("recv failed");
            break;
        }
    }
    close(socketFD);
    return NULL;
}

// Function to create a thread for listening to incoming messages
void createListeningThread(int socketFD) {
    pthread_t id; // Thread identifier
    int* socketFD_ptr = malloc(sizeof(int)); // Allocate memory for the socket file descriptor
    if (socketFD_ptr == NULL) {
        perror("malloc failed");
        exit(EXIT_FAILURE);
    }
    *socketFD_ptr = socketFD; 
    if (pthread_create(&id, NULL, startListening, socketFD_ptr) != 0) {
        perror("Failed to create thread"); 
        free(socketFD_ptr);
    } else {
        pthread_detach(id); // Detach the thread so that its resources are automatically freed when it finishes
    }
}

int main() {
    int socketFD = createTCPIpv4Socket(); // Create a TCP/IPv4 socket
    struct sockaddr_in* address = createIPv4Address("127.0.0.1", 2000); // Create an IPv4 address for localhost and port 2000

    char* name = NULL;
    size_t nameSize = 0;
    printf("What's your name?\n");
    if (getline(&name, &nameSize, stdin) == -1) {
        perror("getline failed"); // Handle input failure
        free(name);
        close(socketFD);
        return 1;
    }

    name[strcspn(name, "\n")] = 0; // Remove the newline character from the name

    int result = connect(socketFD, (struct sockaddr *)address, sizeof(struct sockaddr_in));
    if (result == 0) {
        printf("Connection was successful\n");
    } else {
        perror("Connection failed");
        free(name);
        close(socketFD);
        return 1;
    }

    // Send the name to the server
    if (send(socketFD, name, strlen(name), 0) == -1) {
        perror("send failed");
        free(name);
        close(socketFD);
        return 1;
    }

    char *line = NULL;
    size_t lineSize = 0;
    printf("Type something and it will be sent (type 'exit' to quit)...\n\n");

    createListeningThread(socketFD); // Start the thread to listen for incoming messages

    while (true) {
        ssize_t charCount = getline(&line, &lineSize, stdin);
        if (charCount == -1) {
            perror("getline failed");
            break;
        }

        line[strcspn(line, "\n")] = 0; // Remove the newline character from the input line

        if (strcmp(line, "exit") == 0) {
            break; // Exit the loop if the user types "exit"
        }

        ssize_t amountWasSent = send(socketFD, line, strlen(line), 0); // Send the message to the server
        if (amountWasSent == -1) {
            perror("send failed");
            break;
        }
    }

    free(line);
    free(name);
    close(socketFD);

    return 0;
}
