#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

// Define ANSI escape codes for colors
#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#define PORT 8080  // the port number to connect to the server
#define MAX_MSG 1024  // the maximum message length allowed
#define BUFFER_SIZE 4096  //the buffer size for receiving data from the server

int main() {
    char username[256];  // to store the client's username
    char server_ip[256];  // to store the IP address of the server
    char message[MAX_MSG];  //to store the message that will be sent to the server

    // ask the user to enter their username
    printf("%sEnter your username: %s", KCYN, KNRM);
    scanf("%255s", username);
    getchar();  

    // ask the user to enter the server's IP address
    printf("%sEnter server IP address: %s", KCYN, KNRM);
    scanf("%255s", server_ip); 
    getchar(); 

    // Create a socket using the TCP protocol
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {  // Check if the socket creation failed
        perror(KRED "Socket creation failed" KNRM);  // Print an error message
        return 1;  // Exit with an error status
    }

    struct sockaddr_in remote_addr;  // Structure to hold server address information
    remote_addr.sin_family = AF_INET;  // Set the address family to IPv4
    remote_addr.sin_port = htons(PORT);  // Set the port number, converting it to network byte order

    // Convert the string IP address to a proper format and validate it
    if (!inet_aton(server_ip, &remote_addr.sin_addr)) {
        perror(KRED "Invalid IP address format" KNRM);  // Print an error message if the IP address is invalid
        close(sockfd);  // Close the socket before exiting
        return 1;
    }

    // Attempt to establish a connection to the server
    if (connect(sockfd, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
        perror(KRED "Failed to connect to server" KNRM);  // Print an error message if the connection fails
        close(sockfd);  // Close the socket before exiting
        return 1;
    }

    printf("%sConnected to server at %s!%s\n", KGRN, server_ip, KNRM);  // Notify the user of a successful connection

    while (1) {  // Infinite loop for continuous message exchange
        printf("%s%s@chat%s > ", KGRN, username, KNRM);  // Display the prompt with the username
        fgets(message, MAX_MSG, stdin);  // Read user input from standard input
        message[strcspn(message, "\n")] = 0;  // Remove the newline character from the input
       if (strcmp(message,"\\exit")==0){
          printf("%sExiting...%s\n", KYEL, KNRM);
          break; //exit the program at the command \exit

        }
        if (strlen(message) == 0) {  // Check if the message is empty
            continue;  // continue the loop
        }

        char formatted_msg[MAX_MSG + 256];  // Buffer to hold the formatted message (username + message)
        formatted_msg[0] = '\0';  // Initialize it as an empty string

        // Construct the formatted message: "username: message"
        strncat(formatted_msg, username, sizeof(formatted_msg) - strlen(formatted_msg) - 1);  // Append username
        strncat(formatted_msg, ": ", sizeof(formatted_msg) - strlen(formatted_msg) - 1);  // Append delimiter
        strncat(formatted_msg, message, sizeof(formatted_msg) - strlen(formatted_msg) - 1);  // Append user message

        // Send the formatted message to the server
        int sent = send(sockfd, formatted_msg, strlen(formatted_msg), 0);
        if (sent < 0) {  // Check if sending the message failed
            perror(KRED "Failed to send message" KNRM);  // Print an error message
            break;  // Exit the loop
        }

        // Receive the response from the server
        char buf[BUFFER_SIZE] = {0};  // Buffer for storing the received message
        int rec = recv(sockfd, buf, BUFFER_SIZE - 1, 0);  // Receive data from the server
        if (rec <= 0) {  // Check if the reception failed or the server disconnected
            if (rec == 0) printf("%sServer disconnected.%s\n", KYEL, KNRM);  // Notify the user if the server closed the connection
            else perror(KRED "Failed to receive message" KNRM);  // Print an error message
            break;  // Exit the loop
        }

        buf[rec] = '\0';  // Null-terminate the received message to ensure it is a valid string
        printf("%s[Server]%s %s\n", KBLU, KNRM, buf);  // Print the received message from the server
    }

    close(sockfd);  // Close the socket before exiting to free resources
    return 0;  // Return successful execution status
}
