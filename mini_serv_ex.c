#include <stdlib.h>            
#include <stdio.h> 
#include <string.h>             
#include <unistd.h>             
#include <sys/socket.h>           
#include <netinet/in.h>          
            

// Define a structure 't_client' to represent a client with an ID and a message buffer.
typedef struct client {
    int id;
    char msg[100000];
} t_client;

// Create an array 'clients' to store client data, allowing up to 1024 clients.
t_client clients[1024];

int max = 0, next_id = 0;           // Declare integer variables 'max' and 'next_id' and initialize them to 0.
fd_set active, readyRead, readyWrite; // Declare file descriptor sets 'active', 'readyRead', and 'readyWrite'.
char bufRead[424242], bufWrite[424242]; // Declare character arrays 'bufRead' and 'bufWrite' with specified sizes.

// Function 'exitError' to exit the program with an error message.
void exitError(char *str)
{
    if (str)
        write(2, str, strlen(str)); // Write the error message to standard error (file descriptor 2).
    exit(1); // Exit the program with status 1 (indicating an error).
}

// Function 'sendAll' to send a message to all clients except the one specified.
void sendAll(int es)
{
    for(int i = 0; i <= max; i++)  // Loop through file descriptors from 0 to 'max'.
    {
        if (FD_ISSET(i, &readyWrite) && i != es) // Check if 'i' is in 'readyWrite' and not equal to 'es'.
            send(i, bufWrite, strlen(bufWrite), 0); // Send 'bufWrite' to client with file descriptor 'i'.
    }
}

int main(int argc, char **argv) // Entry point of the program, takes command-line arguments.
{
    if (argc != 2) // Check if the number of command-line arguments is not equal to 2.
        exitError("Wrong number of arguments\n"); // Call 'exitError' with an error message.

    int port = atoi(argv[1]); // Convert the second command-line argument to an integer and store it in 'port'.

    // Create a server socket using IPv4 and TCP.
    int serverSock = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSock < 0) // Check if socket creation fails (returns a negative value).
        exitError("Fatal error\n"); // Call 'exitError' with an error message.

    // Initialize the 'clients' array to zero and set up the 'active' file descriptor set.
    bzero(clients, sizeof(clients)); // Set all 'clients' data to zero.
    FD_ZERO(&active); // Initialize the 'active' file descriptor set as empty.

    max = serverSock; // Set 'max' to the server socket file descriptor.
    FD_SET(serverSock, &active); // Add the server socket to the 'active' set.

    // Configure the server's address.
    struct sockaddr_in addr; // Declare a structure to hold the server's address.
    socklen_t addr_len = sizeof(addr); // Store the size of the address structure.
    addr.sin_family = AF_INET; // Set the address family to IPv4.
    addr.sin_addr.s_addr = (1 << 24) + 127; // Set the IP address to "127.0.0.1" in network order.
    addr.sin_port = htons(port); // Set the port number and convert to network byte order.

    // Bind the server socket to the specified address and port.
    if ((bind(serverSock, (const struct sockaddr *)&addr, sizeof(addr))) < 0)
        exitError("Fatal error\n"); // Call 'exitError' with an error message.
    if (listen(serverSock, 128) < 0) // Listen for incoming connections with a queue size of 128.
        exitError("Fatal error\n"); // Call 'exitError' with an error message.

    while(1) // Infinite loop for handling client connections and data.
    {
        readyRead = readyWrite = active; // Copy 'active' to 'readyRead' and 'readyWrite' for monitoring.

        // Use the select() function to check for ready file descriptors.
        if (select(max + 1, &readyRead, &readyWrite, NULL, NULL) < 0)
            continue; // Continue the loop if select() encounters an error.

        // Loop through file descriptors.
        for(int fd = 0; fd <= max; fd++)
        {
            if (FD_ISSET(fd, &readyRead) && fd == serverSock)
            {
                // Check if the server socket is ready to accept a new client connection.
                int clientSocket = accept(serverSock, NULL, NULL); // Accept a new client connection.
                if (clientSocket < 0) // Check if the accept() call fails.
                    continue; // Continue the loop if accept() fails.

                // Update 'max' if the new client socket has a higher file descriptor.
                max = (clientSocket > max) ? clientSocket : max;

                // Assign a unique ID to the new client and add its socket to 'active'.
                clients[clientSocket].id = next_id++; // Assign the next available ID.
                FD_SET(clientSocket, &active); // Add the new client socket to 'active'.

                // Send a message to all clients about the new client's arrival.
                sprintf(bufWrite, "server: client %d just arrived\n", clients[clientSocket].id);
                sendAll(clientSocket); // Call 'sendAll' to send the message to all clients.
                break; // Exit the loop after handling the new client connection.
            }
            if (FD_ISSET(fd, &readyRead) && fd != serverSock)
            {
                // Check if an existing client socket is ready to receive data.
                int read = recv(fd, bufRead, 424242, 0); // Receive data from the client socket.
                if (read <= 0) // Check if the client has disconnected or an error occurred.
                {
                    // Send a message to all clients about the client's departure.
                    sprintf(bufWrite, "server: client %d just left\n", clients[fd].id);
                    sendAll(fd); // Call 'sendAll' to send the message to all clients.

                    // Remove the client socket from 'active' and close it.
                    FD_CLR(fd, &active); // Remove the client socket from 'active'.
                    close(fd); // Close the client socket.
                    break; // Exit the loop after handling the client's departure.
                }
                else
                {
                    // Process and forward the received message to other clients.
                    for (int i = 0, j = strlen(clients[fd].msg); i < read; i++, j++)
                    {
                        clients[fd].msg[j] = bufRead[i];
                        if (clients[fd].msg[j] == '\n')
                        {
                            clients[fd].msg[j] = '\0';

                            // Create a message with the client's ID and message content.
                            sprintf(bufWrite, "client %d: %s\n", clients[fd].id, clients[fd].msg);

                            sendAll(fd); // Call 'sendAll' to send the message to all clients.
                            bzero(&clients[fd].msg, strlen(clients[fd].msg)); // Clear the message buffer.
                            j = -1; // Reset 'j' to -1 to start building a new message.
                        }
                    }
                    break; // Exit the loop after handling data from the client.
                }
            }
        }
    }
}