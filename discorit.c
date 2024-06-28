#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>

#define PORT 8080
#define IP "127.0.0.1"

void register_user(int sock, const char *username, const char *password) {
    char buffer[1024];
    sprintf(buffer, "REGISTER %s -p %s", username, password);
    send(sock, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    read(sock, buffer, sizeof(buffer));
    printf("%s\n", buffer);
}

void create_room(int sock, const char *room) {
    char buffer[1024];
    sprintf(buffer, "CREATE ROOM %s", room);
    send(sock, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    read(sock, buffer, sizeof(buffer));
    printf("%s\n", buffer);
}

void edit_room(int sock, const char *old_room, const char *new_room) {
    char buffer[1024];
    sprintf(buffer, "EDIT ROOM %s TO %s", old_room, new_room);
    send(sock, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    read(sock, buffer, sizeof(buffer));
    printf("%s\n", buffer);
}

void delete_room(int sock, const char *room) {
    char buffer[1024];
    sprintf(buffer, "DEL ROOM %s", room);
    send(sock, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    read(sock, buffer, sizeof(buffer));
    printf("%s\n", buffer);
}

void delete_all_rooms(int sock) {
    char buffer[1024];
    sprintf(buffer, "DEL ROOM ALL");
    send(sock, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    read(sock, buffer, sizeof(buffer));
    printf("%s\n", buffer);
}

void login_user(int sock, const char *username, const char *password) {
    char buffer[1024];
    sprintf(buffer, "LOGIN %s -p %s", username, password);
    send(sock, buffer, strlen(buffer), 0);
    memset(buffer, 0, sizeof(buffer));
    read(sock, buffer, sizeof(buffer));
    printf("%s\n", buffer);

    // Check if login was successful
    if (strstr(buffer, "username berhasil login") != NULL) {
        char current_username[50];
        strcpy(current_username, username);

        char channel[50] = "";
        char room[50] = "";

        while (1) {
            if (strlen(room) > 0) {
                printf("[%s/%s/%s] ", current_username, channel, room);
            } else if (strlen(channel) > 0) {
                printf("[%s/%s] ", current_username, channel);
            } else {
                printf("[%s] ", current_username);
            }
            
            fgets(buffer, sizeof(buffer), stdin);
            buffer[strcspn(buffer, "\n")] = 0; // Remove newline character

            send(sock, buffer, strlen(buffer), 0);
            memset(buffer, 0, sizeof(buffer));
            read(sock, buffer, sizeof(buffer));
            printf("%s\n", buffer);

            // Check if the response contains "Profil diupdate menjadi"
            if (strstr(buffer, "Profil diupdate menjadi") != NULL) {
                strtok(buffer, " "); // Skip "Profil"
                strtok(NULL, " "); // Skip "diupdate"
                strtok(NULL, " "); // Skip "menjadi"
                char *new_username = strtok(NULL, "\n"); // Get the new username
                strcpy(current_username, new_username);
            } 
        
        if (strstr(buffer, "CREATE ROOM") != NULL) {
                char *room = strtok(buffer + 12, "\n"); // Get room name after "CREATE ROOM "
                create_room(sock, room);
            } else if (strstr(buffer, "EDIT ROOM") != NULL) {
                char *old_room = strtok(buffer + 10, " "); // Get old room name after "EDIT ROOM "
                strtok(NULL, " "); // Skip "TO"
                char *new_room = strtok(NULL, "\n"); // Get new room name after "TO"
                edit_room(sock, old_room, new_room);
            } else if (strstr(buffer, "DEL ROOM ALL") != NULL) {
                delete_all_rooms(sock);
            } else if (strstr(buffer, "DEL ROOM") != NULL) {
                char *room = strtok(buffer + 9, "\n"); // Get room name after "DEL ROOM "
                delete_room(sock, room);
            } else if (strcmp(buffer, "Exited room\n") == 0) {
                // Server responded with "Exited room", so clear room variable
                strcpy(room, "");
            } else if (strcmp(buffer, "Exited channel\n") == 0) {
                // Server responded with "Exited channel", so clear channel and room variables
                strcpy(channel, "");
                strcpy(room, "");
            } else if (strcmp(buffer, "Exited\n") == 0) {
                // Server responded with "Exited", so break the loop and exit the program
                break;
            } else {
                if (strstr(buffer, "joined the channel") != NULL) {
                // Skip the first part "username joined the channel"
                strtok(buffer, " "); // Skip username
                strtok(NULL, " "); // Skip "joined"
                strtok(NULL, " "); // Skip "the"
                strtok(NULL, " "); // Skip "channel"
                char *joined_channel = strtok(NULL, "\n"); // Get the channel name
                strcpy(channel, joined_channel); // Update the channel variable
                strcpy(room, ""); // Reset room name
            } else if (strstr(buffer, "joined the room") != NULL) {
                // Skip the first part "username joined the room"
                strtok(buffer, " "); // Skip username
                strtok(NULL, " "); // Skip "joined"
                strtok(NULL, " "); // Skip "the"
                strtok(NULL, " "); // Skip "room"
                char *joined_room = strtok(NULL, "\n"); // Get the room name
                strcpy(room, joined_room); // Update the room variable
            } else if (strstr(buffer, "butuh key") != NULL) {
                printf("Key: ");
                char key[50];
                fgets(key, sizeof(key), stdin);
                key[strcspn(key, "\n")] = 0; // Remove newline character

                sprintf(buffer, "%s", key);
                send(sock, buffer, strlen(buffer), 0);
                memset(buffer, 0, sizeof(buffer));
                read(sock, buffer, sizeof(buffer));
                printf("%s\n", buffer);

                if (strstr(buffer, "joined the channel") != NULL) {
                    // Skip the first part "username joined the channel"
                    strtok(buffer, " "); // Skip username
                    strtok(NULL, " "); // Skip "joined"
                    strtok(NULL, " "); // Skip "the"
                    strtok(NULL, " "); // Skip "channel"
                    char *joined_channel = strtok(NULL, "\n"); // Get the channel name
                    strcpy(channel, joined_channel); // Update the channel variable
                    strcpy(room, ""); // Reset room name
                }
          }
        }

        }
    }
}



int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s REGISTER|LOGIN username -p password\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer[1024] = {0};

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
    } else if (strcmp(argv[1], "LOGIN") == 0) {
        login_user(sock, argv[2], argv[4]);
    } else {
        fprintf(stderr, "Invalid command\n");
        exit(EXIT_FAILURE);
    }

    close(sock);
    return 0;
}
