#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>

#define PORT 8080
#define MAX_MSG 1024
#define BUFFER_SIZE 4096
#define MAX_USERNAME 256

int main() {
    char username[MAX_USERNAME] = {0};
    char server_ip[MAX_USERNAME] = {0};
    char message[MAX_MSG] = {0};

    printf("Enter your username: ");
    scanf("%255s", username);
    getchar();

    printf("Enter server IP address: ");
    scanf("%255s", server_ip);
    getchar();

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(PORT);

    if (!inet_aton(server_ip, &remote_addr.sin_addr)) {
        perror("Invalid IP address format");
        close(sockfd);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
        perror("Failed to connect to server");
        close(sockfd);
        return 1;
    }

    printf("Connected to server at %s!\n", server_ip);
    printf("You can start sending messages. Type '\\exit' to quit. Type '\\ask' to speak with mistral.\n");

    struct pollfd fds[2] = {
        { 0, POLLIN, 0 },
        { sockfd, POLLIN, 0 }
    };

    while (1) {
        printf("[Client %s] > ", username);
        fflush(stdout);
        
        int poll_result = poll(fds, 2, -1);
        if (poll_result < 0) {
            perror("Poll failed");
            break;
        }

        // Handle input from user
        if (fds[0].revents & POLLIN) {
            memset(message, 0, MAX_MSG);  // Clear previous message
            fgets(message, MAX_MSG, stdin);
            message[strcspn(message, "\n")] = 0;  // Remove newline

            if (strcmp(message, "\\exit") == 0) {
                printf("exiting...\n");
                break;
            }

            if (strlen(message) > 0) {
                char formatted_msg[MAX_MSG + MAX_USERNAME + 2] = {0};
                snprintf(formatted_msg, sizeof(formatted_msg), "%s: %s", username, message);
                
                if (send(sockfd, formatted_msg, strlen(formatted_msg), 0) < 0) {
                    perror("Failed to send message");
                    break;
                }
            }
        }

        // Handle data from server
        if (fds[1].revents & POLLIN) {
            char buf[BUFFER_SIZE] = {0};
            int rec = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
            
            if (rec <= 0) {
                if (rec == 0) printf("Server disconnected.\n");
                else perror("Failed to receive message");
                break;
            }

            buf[rec] = '\0';
            printf("[Server] %s\n", buf);
        }
    }

    close(sockfd);
    return 0;
}