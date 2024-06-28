
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/stat.h>
#include <time.h>
#include <ctype.h>

#define PORT 8080
#define IP "127.0.0.1"
#define BUFFER_SIZE 1024

void *monitor_chat(void *args);

typedef struct {
    int socket;
    char channel[50];
    char room[50];
} MonitorArgs;

void rtrim(char *str) {
    size_t n = strlen(str);
    while (n > 0 && isspace((unsigned char)str[n - 1])) {
        n--;
    }
    str[n] = '\0';
}

void register_user(int sock, const char *username, const char *password) {
    char buffer[BUFFER_SIZE];
    sprintf(buffer, "REGISTER %s -p %s", username, password);
    send(sock, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    read(sock, buffer, sizeof(buffer));
    printf("%s\n", buffer);
}

void login_user(int sock, const char *username, const char *password) {
    char buffer[BUFFER_SIZE];
    sprintf(buffer, "LOGIN %s -p %s", username, password);
    send(sock, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    read(sock, buffer, sizeof(buffer));
    printf("%s\n", buffer);

    // Check if login was successful
    if (strstr(buffer, "berhasil login") == NULL) {
        fprintf(stderr, "Failed to login as monitor\n");
        close(sock);
        exit(EXIT_FAILURE);
    }
}

void *monitor_chat(void *args) {
    MonitorArgs *monitorArgs = (MonitorArgs *)args;
    char buffer[BUFFER_SIZE];
    char chat_file[BUFFER_SIZE];

    sprintf(chat_file, "%s/%s/chat.csv", monitorArgs->channel, monitorArgs->room);

    struct stat file_stat;
    time_t last_mod_time = 0;

    while (1) {
        if (stat(chat_file, &file_stat) == 0) {
            if (file_stat.st_mtime != last_mod_time) {
                last_mod_time = file_stat.st_mtime;

                FILE *file = fopen(chat_file, "r");
                if (file == NULL) {
                    perror("Failed to open chat.csv");
                    continue;
                }

                // Clear the screen and print the chat header
                printf("\n");

                while (fgets(buffer, sizeof(buffer), file)) {
                    char *date = strtok(buffer, ",");
                    char *id_chat = strtok(NULL, ",");
                    char *sender = strtok(NULL, ",");
                    char *chat = strtok(NULL, "\n");

                    // Remove double quotes if present
                    if (chat[0] == '"' && chat[strlen(chat) - 1] == '"') {
                        chat[strlen(chat) - 1] = '\0';
                        chat++;
                    }

                    printf("[%s][%s][%s] \"%s\"\n", date, id_chat, sender, chat);
                    fflush(stdout);
                }
                fclose(file);
                printf("\n"); // Add an extra newline for spacing
            }
        }
        sleep(1); // Check for changes every second
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 5 || (strcmp(argv[1], "LOGIN") != 0 && strcmp(argv[1], "REGISTER") != 0) || strcmp(argv[3], "-p") != 0) {
        fprintf(stderr, "Usage: %s REGISTER|LOGIN <username> -p <password>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    pthread_t tid;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, IP, &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection failed");
        exit(EXIT_FAILURE);
    }

    printf("Connected to server\n");

    if (strcmp(argv[1], "REGISTER") == 0) {
        register_user(sock, argv[2], argv[4]);
        close(sock);
        return 0;
    } else if (strcmp(argv[1], "LOGIN") == 0) {
        login_user(sock, argv[2], argv[4]);
    }

    char channel[50] = "";
    char room[50] = "";

    while (1) {
        printf("[monitor] ");
        fgets(buffer, BUFFER_SIZE, stdin);
        buffer[strcspn(buffer, "\n")] = 0; // Remove newline character

        if (strstr(buffer, "-channel") != NULL && strstr(buffer, "-room") != NULL) {
            char *channel_token = strtok(buffer, " ");
            channel_token = strtok(NULL, " ");
            strcpy(channel, channel_token);
            strtok(NULL, " ");
            char *room_token = strtok(NULL, " ");
            strcpy(room, room_token);

            // Create a thread to monitor the chat
            MonitorArgs *monitorArgs = (MonitorArgs *)malloc(sizeof(MonitorArgs));
            monitorArgs->socket = sock;
            strcpy(monitorArgs->channel, channel);
            strcpy(monitorArgs->room, room);

            if (pthread_create(&tid, NULL, monitor_chat, (void *)monitorArgs) != 0) {
                perror("Thread creation failed");
                free(monitorArgs);
                close(sock);
                exit(EXIT_FAILURE);
            }
            break; // Exit the input loop after starting the monitor
        }
    }

    pthread_join(tid, NULL);
    close(sock);
    return 0;
}
