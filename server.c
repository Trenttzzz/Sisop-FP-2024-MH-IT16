#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdbool.h>
#include <crypt.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define PORT 6000
#define MAX_USERS 100
#define MAX_CHANNELS 50
#define MAX_ROOMS 20
#define MAX_MESSAGE_LENGTH 1024
#define CHANNELS_CSV "./DiscoritIT/channels.csv"
#define USERS_CSV "./DiscoritIT/users.csv"
#define BCRYPT_HASHSIZE 64

// User and Channel structures
typedef struct {
    int id;
    char name[50];
    char password[BCRYPT_HASHSIZE];
    char global_role[10];
} User;

typedef struct {
    int id;
    char name[50];
    char key[BCRYPT_HASHSIZE];
} Channel;

// Global variables
User users[MAX_USERS];
int user_count = 1;

Channel channels[MAX_CHANNELS];
int channel_count = 1;

// bcrypt functions
char* generate_salt()
{
    char salt[] = "$2a$12$XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX";
    const char* charset = "./ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < 22; i++) salt[7 + i] = charset[rand() % 64];
    return strdup(salt);
}

char* bcrypt(const char* password, const char* salt)
{
    char* hashed = crypt(password, salt);
    if (hashed == NULL) 
    {
        perror("crypt");
        exit(EXIT_FAILURE);
    }
    return strdup(hashed);
}

// Function prototypes
void read_users_from_csv();
void write_users_to_csv();
void read_channels_from_csv();
void write_channels_to_csv();
bool register_user(char* username, char* password);
bool login_user(char* username, char* password);
void make_channel(char* name, char* key);
void list_channels(int client_socket);
void join_channel(int client_socket, char* channel_name, char* key);
void make_room(char* channel_name, char* room_name);
void list_rooms(int client_socket, char* channel_name);
void join_room(int client_socket, char* channel_name, char* room_name);
void send_message(int client_socket, char* channel_name, char* room_name, char* message);
void log_activity(char* channel_name, char* activity);

void *handle_client(void *socket_desc) {
    int sock = *(int*)socket_desc;
    char message[MAX_MESSAGE_LENGTH] = {0};
    char logged_in_username[50] = {0};
    bool logged_in = false;

    memset(message, 0, MAX_MESSAGE_LENGTH);
    if (recv(sock, message, MAX_MESSAGE_LENGTH, 0) <= 0) {
        printf("Client disconnected\n");
        close(sock);
        free(socket_desc);
        return NULL;
    }

    char *command = strtok(message, " ");

    if (strcmp(command, "REGISTER") == 0) {
        char *username = strtok(NULL, " ");
        char *dash_p = strtok(NULL, " ");
        char *password = strtok(NULL, " ");
        
        if (username && dash_p && password && strcmp(dash_p, "-p") == 0) {
            if (register_user(username, password)) {
                char response[100];
                snprintf(response, sizeof(response), "%s berhasil register\n", username);
                send(sock, response, strlen(response), 0);
            } else {
                send(sock, "Username sudah terdaftar\n", 26, 0);
            }
        } else {
            send(sock, "Format registrasi salah\n", 25, 0);
        }
        
        close(sock);
        free(socket_desc);
        return NULL;
    } else if (strcmp(command, "LOGIN") == 0) {
        char *username = strtok(NULL, " ");
        char *dash_p = strtok(NULL, " ");
        char *password = strtok(NULL, " ");
        
        if (username && dash_p && password && strcmp(dash_p, "-p") == 0) {
            if (login_user(username, password)) {
                logged_in = true;
                strcpy(logged_in_username, username);
                char response[100];
                snprintf(response, sizeof(response), "%s berhasil login\n", username);
                send(sock, response, strlen(response), 0);
            } else {
                send(sock, "Login gagal\n", 13, 0);
                close(sock);
                free(socket_desc);
                return NULL;
            }
        } else {
            send(sock, "Format login salah\n", 20, 0);
            close(sock);
            free(socket_desc);
            return NULL;
        }
    } else {
        send(sock, "Perintah tidak dikenal\n", 24, 0);
        close(sock);
        free(socket_desc);
        return NULL;
    }

    while (logged_in) {
        memset(message, 0, MAX_MESSAGE_LENGTH);
        if (recv(sock, message, MAX_MESSAGE_LENGTH, 0) <= 0) {
            printf("Client disconnected\n");
            break;
        }

        command = strtok(message, " ");

        char response[MAX_MESSAGE_LENGTH + 100];
        snprintf(response, sizeof(response), "[%s] %s\n", logged_in_username, message);
        send(sock, response, strlen(response), 0);

        if (strcmp(command, "LIST") == 0) {
            char *type = strtok(NULL, " ");
            if (strcmp(type, "CHANNEL") == 0) {
                list_channels(sock);
            } else if (strcmp(type, "ROOM") == 0) {
                char *channel_name = strtok(NULL, " ");
                list_rooms(sock, channel_name);
            }
        } else if (strcmp(command, "JOIN") == 0) {
            char *channel_or_room = strtok(NULL, " ");
            char *key = strtok(NULL, " ");
            if (key == NULL) {
                join_room(sock, NULL, channel_or_room);
            } else {
                join_channel(sock, channel_or_room, key);
            }
        } else if (strcmp(command, "MAKE") == 0) {
            char *type = strtok(NULL, " ");
            char *name = strtok(NULL, " ");
            if (strcmp(type, "CHANNEL") == 0) {
                char *key = strtok(NULL, " ");
                make_channel(name, key);
            } else if (strcmp(type, "ROOM") == 0) {
                char *channel_name = strtok(NULL, " ");
                make_room(channel_name, name);
            }
        } else if (strcmp(command, "QUIT") == 0) {
            break;
        } else {
            send(sock, "Perintah tidak dikenal\n", 24, 0);
        }
    }

    close(sock);
    free(socket_desc);
    return NULL;
}


int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    pthread_t thread_id;

    read_users_from_csv();
    read_channels_from_csv();

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    printf("Server DiscorIT berjalan pada port %d\n", PORT);

    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            exit(EXIT_FAILURE);
        }

        int *new_sock = malloc(sizeof(int));
        *new_sock = new_socket;

        if (pthread_create(&thread_id, NULL, handle_client, (void*)new_sock) < 0) {
            perror("could not create thread");
            return 1;
        }
    }

    return 0;
}

// read users.csv
void read_users_from_csv() {
    FILE *file = fopen(USERS_CSV, "r");
    if (!file) {
        perror("Failed to open users.csv");
        return;
    }

    char line[1024];
    user_count = 0;
    while (fgets(line, sizeof(line), file)) {
        if (user_count == 0) {
            // Skip header line
            continue;
        }
        sscanf(line, "%d,%49[^,],%63[^,],%9[^\n]", 
               &users[user_count].id, 
               users[user_count].name, 
               users[user_count].password, 
               users[user_count].global_role);
        user_count++;
    }
    fclose(file);
}

// Function to write users to CSV
void write_users_to_csv() {
    FILE *file = fopen(USERS_CSV, "w");
    if (!file) {
        perror("Failed to open users.csv");
        return;
    }
    for (int i = 1; i < user_count; i++) {
        fprintf(file, "%d,%s,%s,%s\n", users[i].id, users[i].name, users[i].password, users[i].global_role);
    }
    fclose(file);
}

// Function to read channels from CSV
void read_channels_from_csv() {
    FILE *file = fopen(CHANNELS_CSV, "r");
    if (!file) {
        perror("Failed to open channels.csv");
        return;
    }

    char line[1024];
    channel_count = 0;
    while (fgets(line, sizeof(line), file)) {
        if (channel_count == 0) {
            // Skip header line
            channel_count++;
            continue;
        }
        sscanf(line, "%d,%49[^,],%59[^,]", &channels[channel_count].id, channels[channel_count].name, channels[channel_count].key);
        channel_count++;
    }
    fclose(file);
}

// Function to write channels to CSV
void write_channels_to_csv() {
    FILE *file = fopen(CHANNELS_CSV, "w");
    if (!file) {
        perror("Failed to open channels.csv");
        return;
    }

    fprintf(file, "id,name,key\n");
    for (int i = 0; i < channel_count; i++) {
        fprintf(file, "%d,%s,%s\n", channels[i].id, channels[i].name, channels[i].key);
    }
    fclose(file);
}

bool register_user(char* username, char* password) {
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].name, username) == 0) {
            return false; // User already exists
        }
    }

    users[user_count].id = user_count;
    strncpy(users[user_count].name, username, sizeof(users[user_count].name) - 1);
    users[user_count].name[sizeof(users[user_count].name) - 1] = '\0';
    
    char* salt = generate_salt();
    char* hashed_password = bcrypt(password, salt);
    strncpy(users[user_count].password, hashed_password, sizeof(users[user_count].password) - 1);
    users[user_count].password[sizeof(users[user_count].password) - 1] = '\0';
    
    // Set global_role berdasarkan apakah ini user pertama atau tidak
    if (user_count == 1) {
        strncpy(users[user_count].global_role, "ROOT", sizeof(users[user_count].global_role) - 1);
    } else {
        strncpy(users[user_count].global_role, "USER", sizeof(users[user_count].global_role) - 1);
    }
    users[user_count].global_role[sizeof(users[user_count].global_role) - 1] = '\0';
    
    user_count++;
    write_users_to_csv();
    return true;
}

// Function to login user
bool login_user(char* username, char* password) {
    printf("Attempting to login user: %s\n", username);
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].name, username) == 0) {
            printf("User found: %s\n", users[i].name);
            printf("Stored hashed password: %s\n", users[i].password);
            char* hashed_password = bcrypt(password, users[i].password);
            printf("Computed hashed password: %s\n", hashed_password);
            if (strcmp(hashed_password, users[i].password) == 0) {
                printf("Password match\n");
                return true;
            } else {
                printf("Password does not match\n");
                return false;
            }
        }
    }
    printf("User not found\n");
    return false;
}


// Function to create a new channel
void make_channel(char* name, char* key) {
    for (int i = 0; i < channel_count; i++) {
        if (strcmp(channels[i].name, name) == 0) {
            return; // Channel already exists
        }
    }

    channels[channel_count].id = channel_count;
    strcpy(channels[channel_count].name, name);
    char* salt = generate_salt();
    char* hashed_key = bcrypt(key, salt);
    strcpy(channels[channel_count].key, hashed_key); // Store hashed key
    channel_count++;

    write_channels_to_csv();
}

// Function to list all channels
void list_channels(int client_socket) {
    char buffer[MAX_MESSAGE_LENGTH] = "Channels:\n";
    for (int i = 0; i < channel_count; i++) {
        strcat(buffer, channels[i].name);
        strcat(buffer, "\n");
    }
    send(client_socket, buffer, strlen(buffer), 0);
}

// Function to join a channel
void join_channel(int client_socket, char* channel_name, char* key) {
    for (int i = 0; i < channel_count; i++) {
        if (strcmp(channels[i].name, channel_name) == 0) {
            char* hashed_key = bcrypt(key, channels[i].key);
            if (strcmp(hashed_key, channels[i].key) == 0) {
                send(client_socket, "Berhasil join channel\n", 23, 0);
                return;
            } else {
                send(client_socket, "Kunci salah\n", 13, 0);
                return;
            }
        }
    }
    send(client_socket, "Channel tidak ditemukan\n", 25, 0);
}

// Function to create a new room
void make_room(char* channel_name, char* room_name) {
    // Implement logic to create a room inside a channel
}

// Function to list all rooms in a channel
void list_rooms(int client_socket, char* channel_name) {
    // Implement logic to list all rooms in a channel
}

// Function to join a room
void join_room(int client_socket, char* channel_name, char* room_name) {
    // Implement logic to join a room inside a channel
}

// Function to send a message to a room
void send_message(int client_socket, char* channel_name, char* room_name, char* message) {
    // Implement logic to send a message to a room
}

// Function to log activity in a channel
void log_activity(char* channel_name, char* activity) {
    char path[100];
    sprintf(path, "./channels/%s/log.csv", channel_name);
    FILE *file = fopen(path, "a");
    if (file) {
        fprintf(file, "%s\n", activity);
        fclose(file);
    }
}
