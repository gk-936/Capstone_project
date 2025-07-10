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

#define PORT 8080
#define MAX_MSG 1024
#define BUFFER_SIZE 4096

int main() {
    char username[256];
    printf("%sEnter your username:%s ", KCYN, KNRM);
    scanf("%255s", username);
    getchar();


    // Server socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror(KRED "Socket creation failed" KNRM);
        return 1;
    }

    // Bind to port
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror(KRED "Bind failed" KNRM);
        close(sockfd);
        return 1;
    }

    // Listen for connections
    if (listen(sockfd, 10) < 0) {
        perror(KRED "Listen failed" KNRM);
        close(sockfd);
        return 1;
    }

    printf("%sServer listening on port %d...%s\n", KGRN, PORT, KNRM);


        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int new_sockfd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
        if (new_sockfd < 0) {
            perror(KRED "Accept failed" KNRM);
                    }

        printf("%sConnected to client: %s%s\n", KGRN, inet_ntoa(client_addr.sin_addr), KNRM);

        // Handle client communication in a loop
        while (1) {
            char buf[BUFFER_SIZE] = {0};
            int read_size = recv(new_sockfd, buf, BUFFER_SIZE, 0);
            if (read_size <= 0) {
                if (read_size == 0) {
                    printf("%sClient disconnected.%s\n", KYEL, KNRM);
                } else {
                    perror(KRED "Recv failed" KNRM);
                }
                break;  // Exit the loop if the client disconnects or an error occurs
            }

            // Print the received message
            printf("%s[Client %s]%s %s\n", KBLU, inet_ntoa(client_addr.sin_addr), KNRM, buf);

            // Allow the server operator to type a response
            char server_response[MAX_MSG];
            printf("%sEnter your reply: %s", KCYN, KNRM);
            fgets(server_response, MAX_MSG, stdin);
            server_response[strcspn(server_response, "\n")] = 0;  // Remove newline character if present


            if(strcmp(server_response,"\\exit")==0){
              printf("%sExiting...%s\n", KYEL, KNRM);
              break;//exit the program at the command \exit
            }
            

        char formatted_msg[MAX_MSG + 256];  // Buffer to hold formatted message
        formatted_msg[0] = '\0';  // Initialize to an empty string
        
        strncat(formatted_msg, username, sizeof(formatted_msg) - strlen(formatted_msg) - 1);  // Append username
        strncat(formatted_msg, ": ", sizeof(formatted_msg) - strlen(formatted_msg) - 1);  // Append ": "
        strncat(formatted_msg, server_response, sizeof(formatted_msg) - strlen(formatted_msg) - 1);  // Append message
            




            // Send the server's reply back to the client
            if (send(new_sockfd, formatted_msg, strlen(formatted_msg), 0) < 0) {
                perror(KRED "Failed to send response" KNRM);
                break;
            }

                    }

        close(new_sockfd);  // Close the client connection

    close(sockfd);  // Close the server socket
    return 0;
}
