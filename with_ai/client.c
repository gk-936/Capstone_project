#define _DEFAULT_SOURCE // For inet_aton and other extensions
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
    if (fgets(username, MAX_USERNAME, stdin) == NULL) {
        perror("Error reading username");
        return 1;
    }
    username[strcspn(username, "\n")] = 0; // Remove newline
    if (strlen(username) == 0) {
        fprintf(stderr, "Username cannot be empty.\n");
        return 1;
    }

    printf("Enter server IP address: ");
    if (fgets(server_ip, sizeof(server_ip), stdin) == NULL) {
        perror("Error reading server IP");
        return 1;
    }
    server_ip[strcspn(server_ip, "\n")] = 0; // Remove newline
    if (strlen(server_ip) == 0) {
        fprintf(stderr, "Server IP cannot be empty.\n");
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        return 1;
    }

    struct sockaddr_in remote_addr;
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(PORT);

    if (!inet_aton(server_ip, &remote_addr.sin_addr)) {
        fprintf(stderr, "Invalid IP address format: %s\n", server_ip);
        close(sockfd);
        return 1;
    }

    if (connect(sockfd, (struct sockaddr *)&remote_addr, sizeof(remote_addr)) < 0) {
        perror("Failed to connect to server");
        close(sockfd);
        return 1;
    }

    printf("Connected to server at %s!\n", server_ip);
    printf("Type '\\help' for a list of commands.\n");

    struct pollfd fds[2] = {
        { STDIN_FILENO, POLLIN, 0 }, // Use STDIN_FILENO for clarity
        { sockfd, POLLIN, 0 }
    };

    char current_input[MAX_MSG] = {0}; // Buffer for current user input

    while (1) {
        // Display prompt and current input buffer
        printf("\rYou > %s", current_input);
        fflush(stdout);
        
        int poll_result = poll(fds, 2, -1);
        if (poll_result < 0) {
            perror("Poll failed");
            break;
        }

        // Handle input from user
        if (fds[0].revents & POLLIN) {
            char input_char;
            ssize_t nread = read(STDIN_FILENO, &input_char, 1);

            if (nread > 0) {
                if (input_char == '\n') { // Enter key pressed
                    // Clear the current line before processing message or printing new messages
                    printf("\r%*s\r", (int)(strlen("You > ") + strlen(current_input)), "");
                    fflush(stdout);

                    if (strcmp(current_input, "\\exit") == 0) {
                        printf("Exiting...\n");
                        break;
                    } else if (strcmp(current_input, "\\help") == 0) {
                        printf("Available commands:\n");
                        printf("  \\exit          - Quit the chat.\n");
                        printf("  \\ask <prompt>  - Ask the AI a question.\n");
                        // No need to send \help to server
                    } else if (strlen(current_input) > 0) {
                        char message_to_send[MAX_MSG + MAX_USERNAME + 3]; // username + ": " + message
                        if (strncmp(current_input, "\\ask", 4) == 0) {
                            // Send \ask command as is
                            strncpy(message_to_send, current_input, sizeof(message_to_send) -1);
                             message_to_send[sizeof(message_to_send)-1] = '\0';
                        } else {
                            // Prepend username to regular messages
                            snprintf(message_to_send, sizeof(message_to_send), "%s: %s", username, current_input);
                        }

                        if (send(sockfd, message_to_send, strlen(message_to_send), 0) < 0) {
                            perror("Failed to send message");
                            // Potentially clear current_input or handle error more gracefully
                            break;
                        }
                    }
                    memset(current_input, 0, MAX_MSG); // Clear input buffer after sending/handling
                } else if (input_char == 127 || input_char == 8) { // Handle backspace (ASCII DEL or BS)
                    if (strlen(current_input) > 0) {
                        current_input[strlen(current_input) - 1] = '\0';
                        // Clear the line and redraw
                        printf("\r%*s\r", (int)(strlen("You > ") + strlen(current_input) + 1), "");
                    }
                } else if (strlen(current_input) < MAX_MSG - 1 && input_char >= 32 && input_char <= 126) { // Printable chars
                    current_input[strlen(current_input)] = input_char;
                }
            } else if (nread == 0) { // EOF (e.g., Ctrl+D)
                printf("\r%*s\r", (int)(strlen("You > ") + strlen(current_input)), "");
                printf("Exiting due to EOF...\n");
                break;
            } else { // read error
                perror("Read from stdin failed");
                break;
            }
        }

        // Handle data from server
        if (fds[1].revents & POLLIN) {
            // Clear the current input line before printing server message
            printf("\r%*s\r", (int)(strlen("You > ") + strlen(current_input)), "");
            fflush(stdout);

            char buf[BUFFER_SIZE] = {0};
            int rec = recv(sockfd, buf, BUFFER_SIZE - 1, 0);
            
            if (rec <= 0) {
                if (rec == 0) printf("Server disconnected.\n");
                else perror("Failed to receive message from server");
                break;
            }

            buf[rec] = '\0';
            
            // Check message prefix to determine how to display it
            if (strncmp(buf, "AI|", 3) == 0) {
                printf("[AI] > %s\n", buf + 3);
            } else if (strncmp(buf, "MSG|", 4) == 0) {
                // For messages prefixed with MSG|, we expect "username: actual message"
                // If the server guarantees this format, we can parse username.
                // For now, let's display the whole part after MSG|
                printf("%s\n", buf + 4); // Assumes server sends "username: message" after "MSG|"
            } else {
                // For backward compatibility or messages without specific prefix
                printf("%s\n", buf);
            }
        }
    }

    close(sockfd);
    return 0;
}