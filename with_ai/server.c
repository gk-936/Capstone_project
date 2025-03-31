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
    char *line = strdup(data);
    
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
        perror("Error reading username");
        return 1;
    }
    // Remove newline character if present
    username[strcspn(username, "\n")] = '\0';

    // Validate username length
    if (strlen(username) == 0) {
        fprintf(stderr, "Username cannot be empty\n");
        return 1;
    }

    // Server socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        perror("Socket creation failed");
        return 1;
    }

    // Set socket option to reuse address
    int opt = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(sockfd);
        return 1;
    }

    // Bind to port
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
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
    printf("You can start sending messages. Type '\\exit' to quit. Type '\\ask' to speak with mistral.\n");    
    
    struct pollfd fds[2] = {
        { 0, POLLIN, 0 },       // stdin
        { client_fd, POLLIN, 0 } // client socket
    };
 
    // Main communication loop
    while (1) {
        static int first_prompt = 1;
        if (first_prompt) {
            printf("[Server %s] > ", username);
            fflush(stdout);
            first_prompt = 0;
        }

        int poll_count = poll(fds, 2, -1);
        if (poll_count < 0) {
            perror("Poll failed");
            break;
        }

        // Handle user input from stdin
        if (fds[0].revents & POLLIN) {
            memset(message, 0, MAX_MSG);  // Clear previous message
            if (fgets(message, MAX_MSG - 1, stdin) == NULL) {
                perror("Error reading message");
                break;
            }
            message[strcspn(message, "\n")] = 0;  // Remove newline

            if (strcmp(message, "\\exit") == 0) {
                printf("Exiting...\n");
                break;  // Exit the loop if the server operator types \exit
            }
            
            if (strncmp(message, "\\ask", 4) == 0) {
                char *prompt = message + 4;
                while (*prompt && isspace((unsigned char)*prompt)) prompt++;
                
                if (strlen(prompt) > 0) {
                    char *ai_response = get_ai_response(prompt);
                    if (ai_response) {
                        // Print response only to server
                        printf("\nAI: %s\n", ai_response);
                        free(ai_response);
                    } else {
                        fprintf(stderr, "Failed to get AI response\n");
                    }
                } else {
                    fprintf(stderr, "Empty prompt after \\ask\n");
                }
                continue;
            }
            
            // Send regular message to client with MSG prefix
            if (strlen(message) > 0) {
                char formatted_msg[MAX_MSG + MAX_USERNAME + 10] = {0}; // Added extra space for prefix
                snprintf(formatted_msg, sizeof(formatted_msg), "MSG|%s: %s", username, message);
                if (send(client_fd, formatted_msg, strlen(formatted_msg), 0) < 0) {
                    perror("Failed to send message");
                    break;
                }
            }

            // Print prompt only after processing user input
            printf("[Server %s] > ", username);
            fflush(stdout);
        }
        
        // Handle messages from client
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
            
            // Check if client sent \ask command
            if (strncmp(buf, "\\ask", 4) == 0) {
                // Don't print the AI request to server console
                char *prompt = buf + 4;
                while (*prompt && isspace((unsigned char)*prompt)) prompt++;
                
                if (strlen(prompt) > 0) {
                    char *ai_response = get_ai_response(prompt);
                    if (ai_response) {
                        char formatted_response[BUFFER_SIZE] = {0};
                        snprintf(formatted_response, sizeof(formatted_response), "AI|%s", ai_response);
                        if (send(client_fd, formatted_response, strlen(formatted_response), 0) < 0) {
                            perror("Failed to send AI response");
                        }
                        free(ai_response);
                    } else {
                        fprintf(stderr, "Failed to get AI response for client\n");
                    }
                } else {
                    fprintf(stderr, "Empty prompt from client\n");
                }
            } else {
                // Only print regular messages from client
                printf("\n[Client] %s\n", buf);
            }

            // Only print prompt after regular messages, not after AI requests
            if (strncmp(buf, "\\ask", 4) != 0) {
                printf("[Server %s] > ", username);
                fflush(stdout);
            }
        }
    }
               
    // Clean up
    close(client_fd);
    close(sockfd);
    return 0;
}