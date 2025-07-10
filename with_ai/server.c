#define _DEFAULT_SOURCE // For strdup and other POSIX extensions
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <poll.h>
#include <curl/curl.h>
#include <jansson.h>
#include <ctype.h>

#define PORT 8080
#define MAX_MSG 1024
#define BUFFER_SIZE 4096
#define MAX_USERNAME 256

typedef struct {
    char *data;
    size_t size;
} Buffer;

// Initialize buffer
static void buffer_init(Buffer *buffer) {
    buffer->data = malloc(1);
    if (buffer->data) {
        buffer->data[0] = '\0';
        buffer->size = 0;
    }
}

// Append data to buffer
static size_t buffer_append(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    Buffer *buffer = (Buffer *)userp;
    
    char *ptr = realloc(buffer->data, buffer->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "Memory allocation failed\n");
        return 0;
    }
    
    buffer->data = ptr;
    memcpy(&(buffer->data[buffer->size]), contents, realsize);
    buffer->size += realsize;
    buffer->data[buffer->size] = '\0';
    
    return realsize;
}

// Callback function for streaming AI responses
static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    char *data = (char *)contents;
    char *line = strdup(data); // strdup is POSIX, needs _DEFAULT_SOURCE or _GNU_SOURCE
    
    if (!line) {
        fprintf(stderr, "Memory allocation failed\n");
        return 0;
    }
    
    char *ptr = line;

    // Split the response into lines
    while (*ptr) {
        char *end = strchr(ptr, '\n');
        if (end) *end = '\0';

        if (strlen(ptr) > 0) {
            json_error_t error;
            json_t *root = json_loads(ptr, 0, &error);
            if (root) {
                json_t *response = json_object_get(root, "response");
                if (response && json_is_string(response)) {
                    const char *text = json_string_value(response);
                    
                    // Only store in buffer, don't print
                    Buffer *buffer = (Buffer *)userp;
                    if (buffer && buffer->data) {
                        size_t new_size = buffer->size + strlen(text) + 1;
                        char *new_data = realloc(buffer->data, new_size);
                        if (new_data) {
                            buffer->data = new_data;
                            strcat(buffer->data, text);
                            buffer->size = strlen(buffer->data);
                        }
                    }
                }
                json_decref(root);
            }
        }

        if (!end) break;
        ptr = end + 1;
    }

    free(line);
    return realsize;
}

// Function to get AI response
char* get_ai_response(const char* prompt) {
    CURL *curl;
    CURLcode res;
    struct curl_slist *headers = NULL;
    Buffer buffer;
    
    // Initialize buffer
    buffer_init(&buffer);
    if (!buffer.data) {
        fprintf(stderr, "Failed to initialize buffer\n");
        return NULL;
    }
    
    // Create JSON payload
    json_t *root = json_object();
    if (!root) {
        fprintf(stderr, "Failed to create JSON object\n");
        free(buffer.data);
        return NULL;
    }
    
    json_object_set_new(root, "model", json_string("mistral"));
    json_object_set_new(root, "prompt", json_string(prompt));
    char *json_data = json_dumps(root, JSON_COMPACT);
    json_decref(root);
    
    if (!json_data) {
        fprintf(stderr, "Failed to dump JSON data\n");
        free(buffer.data);
        return NULL;
    }
    
    curl = curl_easy_init();
    if (curl) {
        headers = curl_slist_append(NULL, "Content-Type: application/json");
        
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:11434/api/generate");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, json_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
        
        res = curl_easy_perform(curl);
        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            free(buffer.data);
            free(json_data);
            curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            return NULL;
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    } else {
        fprintf(stderr, "Failed to initialize curl\n");
        free(buffer.data);
        free(json_data);
        return NULL;
    }
    
    free(json_data);
    return buffer.data;  // Return the buffer containing the full response
}

int main() {
    char username[MAX_USERNAME] = {0};
    char message[MAX_MSG] = {0};
  
    printf("Enter your username: ");
    if (fgets(username, MAX_USERNAME - 1, stdin) == NULL) {
        perror("[INFO] Error reading username");
        return 1;
    }
    username[strcspn(username, "\n")] = '\0';

    if (strlen(username) == 0) {
        fprintf(stderr, "[INFO] Username cannot be empty\n");
        return 1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("[INFO] Socket creation failed");
        return 1;
    }

    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("[INFO] setsockopt failed");
        close(sockfd);
        return 1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[INFO] Bind failed");
        close(sockfd);
        return 1;
    }

    if (listen(sockfd, 10) < 0) {
        perror("[INFO] Listen failed");
        close(sockfd);
        return 1;
    }

    printf("[INFO] Server listening on port %d as '%s'...\n", PORT, username);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(sockfd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        perror("[INFO] Accept failed");
        close(sockfd);
        return 1;
    }

    printf("[INFO] Client %s connected.\n", inet_ntoa(client_addr.sin_addr));
    printf("[INFO] Type '\\exit' to stop the server. Type '\\ask <prompt>' to query AI.\n");
    
    struct pollfd fds[2] = {
        { STDIN_FILENO, POLLIN, 0 }, // stdin
        { client_fd, POLLIN, 0 }     // client socket
    };

    char current_input[MAX_MSG] = {0}; // Buffer for server's own input

    // Main communication loop
    while (1) {
        printf("\rServer (%s) > %s", username, current_input);
        fflush(stdout);

        int poll_count = poll(fds, 2, -1);
        if (poll_count < 0) {
            perror("[INFO] Poll failed");
            break;
        }

        // Handle server operator input from stdin
        if (fds[0].revents & POLLIN) {
            char input_char;
            ssize_t nread = read(STDIN_FILENO, &input_char, 1);

            if (nread > 0) {
                if (input_char == '\n') { // Enter key pressed
                    printf("\r%*s\r", (int)(strlen("Server () > ") + strlen(username) + strlen(current_input)), "");
                    fflush(stdout);

                    if (strcmp(current_input, "\\exit") == 0) {
                        printf("[INFO] Server exiting...\n");
                        break;
                    } else if (strncmp(current_input, "\\ask", 4) == 0) {
                        char *prompt = current_input + 4;
                        while (*prompt && isspace((unsigned char)*prompt)) prompt++;

                        if (strlen(prompt) > 0) {
                            printf("[AI-LOG] Server initiated AI query: %s\n", prompt);
                            char *ai_response = get_ai_response(prompt);
                            if (ai_response) {
                                printf("[AI] > %s\n", ai_response); // Display AI response to server console
                                free(ai_response);
                            } else {
                                fprintf(stderr, "[AI-LOG] Failed to get AI response for server query.\n");
                            }
                        } else {
                            fprintf(stderr, "[INFO] Empty prompt after \\ask command.\n");
                        }
                    } else if (strlen(current_input) > 0) {
                        // Server operator sends a message to the client
                        char formatted_msg[MAX_MSG + MAX_USERNAME + 15] = {0}; // Increased buffer
                        // Prefix with "MSG|" and server's username
                        snprintf(formatted_msg, sizeof(formatted_msg), "MSG|Server (%s): %s",username, current_input);
                        if (send(client_fd, formatted_msg, strlen(formatted_msg), 0) < 0) {
                            perror("[INFO] Failed to send message to client");
                            // No break here, server should continue running
                        }
                        printf("You (Server %s) > %s\n", username, current_input); // Log server's own message
                    }
                    memset(current_input, 0, MAX_MSG); // Clear input buffer
                } else if (input_char == 127 || input_char == 8) { // Backspace
                    if (strlen(current_input) > 0) {
                        current_input[strlen(current_input) - 1] = '\0';
                        printf("\r%*s\r", (int)(strlen("Server () > ") + strlen(username) + strlen(current_input) + 1), "");
                    }
                } else if (strlen(current_input) < MAX_MSG - 1 && input_char >= 32 && input_char <=126) {
                    current_input[strlen(current_input)] = input_char;
                }
            } else if (nread == 0) { // EOF
                printf("\r%*s\r", (int)(strlen("Server () > ") + strlen(username) + strlen(current_input)), "");
                printf("[INFO] EOF received, server exiting...\n");
                break;
            } else {
                perror("[INFO] Read from stdin failed");
                break;
            }
        }
        
        // Handle messages from client
        if (fds[1].revents & POLLIN) {
            printf("\r%*s\r", (int)(strlen("Server () > ") + strlen(username) + strlen(current_input)), "");
            fflush(stdout);

            char buf[BUFFER_SIZE] = {0};
            int rec = recv(client_fd, buf, BUFFER_SIZE - 1, 0);

            if (rec <= 0) {
                if (rec == 0) {
                    printf("[INFO] Client disconnected.\n");
                } else {
                    perror("[INFO] Recv from client failed");
                }
                // Don't break the server if one client disconnects, could wait for new one
                // For this simple 1-1 chat, we break.
                break;
            }
            buf[rec] = '\0';
            
            if (strncmp(buf, "\\ask", 4) == 0) {
                char *prompt = buf + 4;
                while (*prompt && isspace((unsigned char)*prompt)) prompt++;
                
                printf("[AI-LOG] Client initiated AI query: %s\n", prompt);
                if (strlen(prompt) > 0) {
                    char *ai_response = get_ai_response(prompt);
                    if (ai_response) {
                        char formatted_response[BUFFER_SIZE] = {0};
                        // Prefix with "AI|" for client to parse
                        snprintf(formatted_response, sizeof(formatted_response), "AI|%s", ai_response);
                        if (send(client_fd, formatted_response, strlen(formatted_response), 0) < 0) {
                            perror("[INFO] Failed to send AI response to client");
                        }
                        // Optionally log AI response on server too, or part of it
                        // printf("[AI-LOG] Sent AI response to client (first 50 chars): %.50s...\n", ai_response);
                        free(ai_response);
                    } else {
                        fprintf(stderr, "[AI-LOG] Failed to get AI response for client query.\n");
                        // Inform client about failure?
                        char *fail_msg = "AI|Sorry, I could not process your request.";
                        send(client_fd, fail_msg, strlen(fail_msg),0);
                    }
                } else {
                    fprintf(stderr, "[INFO] Empty prompt from client's \\ask command.\n");
                    char *empty_prompt_msg = "AI|Your \\ask command was empty.";
                    send(client_fd, empty_prompt_msg, strlen(empty_prompt_msg),0);
                }
            } else {
                // Assumes client sends "username: message"
                // For now, server prints it as is, but prefixed.
                // The client side change was to send "username: message" for non-\ask messages.
                printf("[Client] %s\n", buf);
            }
        }
    }
               
    // Clean up
    close(client_fd);
    close(sockfd);
    return 0;
}