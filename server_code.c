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
    char message[MAX_MSG] = {0};
    printf("Enter your username:");
    scanf("%255s", username);
    getchar();


    // Server socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // Bind to port
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("Bind failed");
        close(sockfd);
        return 1;
    }

    // Listen for connections
    if (listen(sockfd, 10) < 0) {
        perror("Listen failed");
        close(sockfd);
        return 1;
    }

    printf("Server listening on port %d...\n", PORT);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("Accept failed");
        close(sockfd);
        return 1;
    }

    printf("Connected to client: %s\n", inet_ntoa(client_addr.sin_addr));
        
    struct pollfd fds[2]= {
        { 0, POLLIN, 0 },  // stdin
        { client_fd, POLLIN, 0 }  // client socket
    };
 

    while (1) {

        printf("[Server %s] > ", username);
        fflush(stdout);  // Ensure the prompt is displayed immediately

        int poll_count = poll(fds, 2, -1);  // Wait indefinitely for events
        if (poll_count < 0) {
            perror("Poll failed");
            break;
        }

        if (fds[0].revents & POLLIN) {
            memset(message,0, MAX_MSG);  // Clear previous message
            fgets(message, MAX_MSG, stdin);
            message[strcspn(message, "\n")] = 0;  // Remove newline

            if (strcmp(message, "\\exit") == 0) {
                printf("Exiting...\n");
                break;  // Exit the loop if the server operator types \exit
            }

            if (strlen(message) > 0) {
                char formatted_msg[MAX_MSG + MAX_USERNAME + 2] = {0};
                snprintf(formatted_msg, sizeof(formatted_msg), "%s: %s", username, message);
                if (send(client_fd, formatted_msg, strlen(formatted_msg), 0) < 0) {
                    perror("Failed to send message");
                    break;
                }
            }

        }
        if (fds[1].revents & POLLIN) {
            char buf[BUFFER_SIZE] = {0};
            int rec = recv(client_fd, buf, BUFFER_SIZE - 1, 0);

            if (rec <= 0) {
                if (rec == 0) {
                    printf("**Client disconnected.**\n");
                } else {
                    perror("Recv failed");
                }
                break;  // Exit the loop if the client disconnects or an error occurs
            }
            buf[rec] = '\0';
            printf("\n[client] %s\n",buf);
        
        }
    }
               
    close(client_fd);
    close(sockfd);
    return 0;

}
