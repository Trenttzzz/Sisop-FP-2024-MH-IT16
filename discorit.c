#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

#define PORT 6000
#define BUFFER_SIZE 1024

int server_fd;
char logged_in_username[50] = {0};
pthread_mutex_t print_mutex = PTHREAD_MUTEX_INITIALIZER;

void connect_to_server() {
    struct sockaddr_in serv_addr;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation error");
        exit(EXIT_FAILURE);
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        perror("Invalid address/ Address not supported");
        exit(EXIT_FAILURE);
    }

    if (connect(server_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Connection Failed");
        exit(EXIT_FAILURE);
    }
}

void *receive_handler(void *arg) {
    char buffer[BUFFER_SIZE];
    while (1) {
        memset(buffer, 0, BUFFER_SIZE);
        int recv_size = recv(server_fd, buffer, BUFFER_SIZE - 1, 0);
        if (recv_size <= 0) {
            pthread_mutex_lock(&print_mutex);
            printf("Disconnected from server.\n");
            pthread_mutex_unlock(&print_mutex);
            exit(0);
        }
        pthread_mutex_lock(&print_mutex);
        printf("%s", buffer);
        if (strcmp(buffer, "Disconnected from server.\n") != 0) {
            printf("[%s] ", logged_in_username);
        }
        fflush(stdout);
        pthread_mutex_unlock(&print_mutex);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <command> [args...]\n", argv[0]);
        return 1;
    }

    connect_to_server();

    char command[BUFFER_SIZE] = {0};
    for (int i = 1; i < argc; i++) {
        strcat(command, argv[i]);
        if (i < argc - 1) strcat(command, " ");
    }

    send(server_fd, command, strlen(command), 0);

    char response[BUFFER_SIZE] = {0};
    recv(server_fd, response, BUFFER_SIZE - 1, 0);

    if (strncmp(command, "LOGIN", 5) == 0) {
        if (strstr(response, "berhasil login") != NULL) {
            sscanf(response, "%s berhasil login", logged_in_username);
            printf("[%s] ", logged_in_username);
            fflush(stdout);

            pthread_t recv_thread;
            pthread_create(&recv_thread, NULL, receive_handler, NULL);

            while (1) {
                char input[BUFFER_SIZE] = {0};
                if (fgets(input, BUFFER_SIZE, stdin) == NULL) break;
                input[strcspn(input, "\n")] = 0;

                if (strcmp(input, "QUIT") == 0) {
                    send(server_fd, input, strlen(input), 0);
                    break;
                }

                send(server_fd, input, strlen(input), 0);
            }

            pthread_cancel(recv_thread);
            pthread_join(recv_thread, NULL);
        } else {
            printf("%s", response);
        }
    } else {
        printf("%s", response);
    }

    close(server_fd);
    return 0;
}