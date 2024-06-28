#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <bcrypt.h>
#include <time.h>
#include <ctype.h>
#include <sys/stat.h>
#include <dirent.h>

#define PORT 8080
#define MAX_CLIENTS 100
// #define BCRYPT_HASHSIZE 64
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    int role; // 0 = USER, 1 = ROOT
    char username[50];
    char current_channel[50]; // Current channel the user is in
    char current_room[50];    // Current room the user is in
} Client;


Client clients[MAX_CLIENTS];
int client_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// Fungsi untuk menghapus newline atau karakter whitespace lainnya dari akhir string
void rtrim(char *str) {
    size_t n = strlen(str);
    while (n > 0 && isspace((unsigned char)str[n - 1])) {
        n--;
    }
    str[n] = '\0';
}

int remove_directory(const char *path) {
    DIR *d = opendir(path);
    size_t path_len = strlen(path);
    int r = -1;

    if (d) {
        struct dirent *p;
        r = 0;
        while (!r && (p = readdir(d))) {
            int r2 = -1;
            char *buf;
            size_t len;

            // Lewati entri "." dan ".."
            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
                continue;
            }

            len = path_len + strlen(p->d_name) + 2;
            buf = malloc(len);
            if (buf) {
                struct stat statbuf;
                snprintf(buf, len, "%s/%s", path, p->d_name);
                if (!stat(buf, &statbuf)) {
                    if (S_ISDIR(statbuf.st_mode)) {
                        r2 = remove_directory(buf);
                    } else {
                        r2 = unlink(buf);
                    }
                }
                free(buf);
            }
            r = r2;
        }
        closedir(d);
    }

    if (!r) {
        r = rmdir(path);
    }

    return r;
}

void delete_directory(const char *path) {
    struct dirent *entry;
    DIR *dir = opendir(path);

    if (dir == NULL) {
        return;
    }

    char full_path[BUFFER_SIZE];
    struct stat statbuf;

    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", path, entry->d_name);
        if (stat(full_path, &statbuf) == 0) {
            if (S_ISDIR(statbuf.st_mode)) {
                delete_directory(full_path);
            } else {
                remove(full_path);
            }
        }
    }

    closedir(dir);
    rmdir(path);
}

void load_users();
void register_user(const char* username, const char* password, int client_socket);
void login_user(const char* username, const char* password, int client_socket);
void create_channel(const char* username, const char* channel, const char* key, int client_socket);
void edit_channel(const char* username, const char* old_channel, const char* new_channel, int client_socket);
void delete_channel(const char* username, const char* channel, int client_socket);
void join_channel_without_key(const char *username, const char *channel, int client_socket);
void join_channel_with_key(const char *username, const char *channel, const char *key, int client_socket);
int validate_csv_auth(const char *username, const char *channel, int client_socket);
int validate_csv_auth_admin(const char *username, const char *channel);
int validate_csv_users(const char *username, int client_socket);
void list_channel(int client_socket);
void list_users_in_channel(const char *channel, int client_socket);
void create_room(const char *username, const char *channel, const char *room, int client_socket);
void join_room(const char *username, const char *channel, const char *room, int client_socket);
void edit_room(const char *username, const char *channel, const char *old_room, const char *new_room, int client_socket);
void delete_room(const char *username, const char *channel, const char *room, int client_socket);
void delete_all_rooms(const char *username, const char *channel, int client_socket);
void list_room(const char *channel, int client_socket);
void send_chat(const char *username, const char *channel, const char *room, const char *message, int client_socket);
void see_chat(const char *channel, const char *room, int client_socket);
void edit_chat(const char *channel, const char *room, int id_chat, const char *new_message, int client_socket);
void delete_chat(const char *channel, const char *room, int id_chat, int client_socket);
void list_users(int client_socket);
void edit_username(const char* current_username, const char* new_username, int client_socket);
void edit_password(const char* username, const char* new_password, int client_socket);
void edit_own_username(const char* current_username, const char* new_username, int client_socket);
void edit_own_password(const char* username, const char* new_password, int client_socket);
void unban_user(const char *admin, const char *channel, const char *user_to_unban, int client_socket);
void ban_user(const char *admin, const char *channel, const char *user_to_ban, int client_socket); 
void remove_user(const char* username, int client_socket);
void remove_user_from_channel(const char *admin, const char *channel, const char *user_to_remove, int client_socket);
void send_response(int client_socket, const char *message);

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int n;
    char channel[50] = "";

    while ((n = read(client_socket, buffer, sizeof(buffer))) > 0) {
        buffer[n] = '\0';
        printf("Client: %s\n", buffer);

        char *command = strtok(buffer, " ");
        if (strcmp(command, "REGISTER") == 0) {
            char *username = strtok(NULL, " ");
            strtok(NULL, " "); // skip "-p"
            char *password = strtok(NULL, " ");
            register_user(username, password, client_socket);
        } 

        else if (strcmp(command, "LOGIN") == 0) {
            char *username = strtok(NULL, " ");
            strtok(NULL, " "); // skip "-p"
            char *password = strtok(NULL, " ");
            login_user(username, password, client_socket);
        } 

        else if (strcmp(command, "CREATE") == 0) {
            char *sub_command = strtok(NULL, " ");

            if (strcmp(sub_command, "CHANNEL") == 0) {
                char *channel = strtok(NULL, " ");
                
                if (channel == NULL || strcmp(strtok(NULL, " "), "-k") != 0) {
                    send_response(client_socket, "Command salah: CREATE CHANNEL <channel_name> -k <key>\n");
                }

                strtok(NULL, " "); // skip "-k"
                char *key = strtok(NULL, " ");
                create_channel(clients[client_socket].username, channel, key, client_socket);
            } else if (strcmp(sub_command, "ROOM") == 0) {
                char *room = strtok(NULL, " ");
                if (room == NULL) {
                    send_response(client_socket, "Command salah: CREATE ROOM <room_name>\n");
                }
                create_room(clients[client_socket].username, clients[client_socket].current_channel, room, client_socket);
            } else {
                send_response(client_socket, "Command salah: CREATE <CHANNEL/ROOM> <name> [-k <key>]\n");
            }
        }
        
        else if (strcmp(command, "CHAT") == 0) {
            char *message = strtok(NULL, "\"");
            if (message != NULL) {
                send_chat(clients[client_socket].username, clients[client_socket].current_channel, clients[client_socket].current_room, message, client_socket);
            } else {
                send_response(client_socket, "Pesan tidak valid\n");
            }
        } 
        
        else if (strcmp(command, "EDIT") == 0) {
            char *sub_command = strtok(NULL, " ");
            if (strcmp(sub_command, "CHANNEL") == 0) {
                char *old_channel = strtok(NULL, " ");
                strtok(NULL, " "); // skip "TO"

                if (old_channel == NULL || strcmp(strtok(NULL, " "), "TO") != 0) {
                    send_response(client_socket, "Command salah: EDIT CHANNEL <old_channel_name> TO <new_channel_name>\n");
                }

                char *new_channel = strtok(NULL, " ");
                edit_channel(clients[client_socket].username, old_channel, new_channel, client_socket);
            } else if (strcmp(sub_command, "ROOM") == 0) {
                char *old_room = strtok(NULL, " ");
                if (old_room == NULL || strcmp(strtok(NULL, " "), "TO") != 0) {
                    send_response(client_socket, "Command salah: EDIT ROOM <old_room_name> TO <new_room_name>\n");
                }

                strtok(NULL, " "); // skip "TO"
                char *new_room = strtok(NULL, " ");
                edit_room(clients[client_socket].username, clients[client_socket].current_channel, old_room, new_room, client_socket);
            } else if (strcmp(sub_command, "CHAT") == 0) {
                int id_chat = atoi(strtok(NULL, " "));
                char *new_message = strtok(NULL, "\"");
                if (new_message != NULL) {
                    edit_chat(clients[client_socket].current_channel, clients[client_socket].current_room, id_chat, new_message, client_socket);
                } else {
                    send_response(client_socket, "Pesan tidak valid\n");
                }
            } else if (strcmp(sub_command, "WHERE") == 0) {
                char *target_user = strtok(NULL, " ");
                char *edit_option = strtok(NULL, " ");
                if (strcmp(edit_option, "-u") == 0) {
                    char *new_username = strtok(NULL, " ");
                    edit_username(target_user, new_username, client_socket);
                } else if (strcmp(edit_option, "-p") == 0) {
                    char *new_password = strtok(NULL, " ");
                    edit_password(target_user, new_password, client_socket);
                }
            } else if (strcmp(sub_command, "PROFILE") == 0) {
                char *profile_command = strtok(NULL, " ");
                if (strcmp(profile_command, "SELF") == 0) {
                    char *edit_option = strtok(NULL, " ");
                    if (strcmp(edit_option, "-u") == 0) {
                        char *new_username = strtok(NULL, " ");
                        edit_own_username(clients[client_socket].username, new_username, client_socket);
                    } else if (strcmp(edit_option, "-p") == 0) {
                        char *new_password = strtok(NULL, " ");
                        edit_own_password(clients[client_socket].username, new_password, client_socket);
                    }
                }
            } else {
                send_response(client_socket, "Command salah: EDIT <CHANNEL/ROOM/CHAT/PROFILE>\n");
            }
        } else if (strcmp(command, "DEL") == 0) {
                char *sub_command = strtok(NULL, " ");
                if (strcmp(sub_command, "CHANNEL") == 0) {
                    char *channel = strtok(NULL, " ");
                    delete_channel(clients[client_socket].username, channel, client_socket);
                } else if (strcmp(sub_command, "ROOM") == 0) {
                    char *room = strtok(NULL, " ");
                    if (strcmp(room, "ALL") == 0) {
                        delete_all_rooms(clients[client_socket].username, clients[client_socket].current_channel, client_socket);
                    } else {
                        delete_room(clients[client_socket].username, clients[client_socket].current_channel, room, client_socket);
                    }
                } else if (strcmp(sub_command, "CHAT") == 0) {
                    int id_chat = atoi(strtok(NULL, " "));
                    delete_chat(clients[client_socket].current_channel, clients[client_socket].current_room, id_chat, client_socket);
                } else {
                    send_response(client_socket, "Command salah: DEL <CHANNEL/ROOM/CHAT>\n");
                }
        } else if (strcmp(command, "SEE") == 0) {
                char *sub_command = strtok(NULL, " ");
                if (strcmp(sub_command, "CHAT") == 0) {
                    see_chat(clients[client_socket].current_channel, clients[client_socket].current_room, client_socket);
                } else {
                    send_response(client_socket, "Command salah: SEE <CHAT>\n");
                }
        } else if (strcmp(command, "JOIN") == 0) {
            char *temp_entity = strtok(NULL, " ");
            strcpy(channel, temp_entity);

            if (strlen(clients[client_socket].current_channel) > 0) {
                if (temp_entity == NULL) {
                    send_response(client_socket, "Command salah: JOIN <room_name>\n");
                }
                // User is already in a channel, so this must be a room join request
                join_room(clients[client_socket].username, clients[client_socket].current_channel, channel, client_socket);
            }  else {  // User is not in a channel, so this must be a channel join request
                if (temp_entity == NULL) {
                    send_response(client_socket, "Command salah: JOIN <channel_name>\n");
                }

                if (channel != NULL) {
                    printf("Debug: User %s joining channel %s\n", clients[client_socket].username, channel); // Debugging output
                    int is_admin = validate_csv_auth(clients[client_socket].username, channel, client_socket);

                    if (is_admin == 1) {
                        printf("Debug: User %s is admin in channel %s\n", clients[client_socket].username, temp_entity); // Debugging output
                        join_channel_without_key(clients[client_socket].username, temp_entity, client_socket);
                    } else if (is_admin == -1) {
                        send_response(client_socket, "anda telah diban, silakan menghubungi admin\n");
                        return;  
                    }
                    
                    else {
                        int is_root = validate_csv_users(clients[client_socket].username, client_socket);
                        if (is_root == 1) {
                            printf("Debug: User %s is ROOT\n", clients[client_socket].username); // Debugging output
                            join_channel_without_key(clients[client_socket].username, temp_entity, client_socket);
                        } 
                        
                        else {
                            printf("Debug: User %s is not admin or ROOT, needs key\n", clients[client_socket].username); // Debugging output
                            send_response(client_socket, "butuh key\n");
                            memset(buffer, 0, sizeof(buffer));
                            n = read(client_socket, buffer, sizeof(buffer));
                            buffer[n] = '\0';
                            printf("Debug: Received key %s for channel %s\n", buffer, channel); // Debugging output
                            join_channel_with_key(clients[client_socket].username, channel, buffer, client_socket);
                        }
                    }
                } else {
                    send_response(client_socket, "Channel tidak valid\n");
                }
            }
        } else if (strcmp(command, "LIST") == 0) {
            char *sub_command = strtok(NULL, " ");
            if (strcmp(sub_command, "CHANNEL") == 0) {
                list_channel(client_socket);
            } else if (strcmp(sub_command, "ROOM") == 0) {
                list_room(clients[client_socket].current_channel, client_socket);
            } else if (strcmp(sub_command, "USER") == 0) {
                if (strlen(clients[client_socket].current_channel) > 0) {
                    // User is already in a channel, so this must be a room list request
                    list_users_in_channel(clients[client_socket].current_channel, client_socket);
                } else {
                    list_users(client_socket);
                } 
            } else {
                send_response(client_socket, "Command salah: LIST <CHANNEL/ROOM/USER>\n");
            }
        } else if (strcmp(command, "EXIT") == 0) {
            if (strlen(clients[client_socket].current_room) > 0) {
                // Exiting room
                memset(clients[client_socket].current_room, 0, sizeof(clients[client_socket].current_room));
                send_response(client_socket, "Exited room\n");
            } else if (strlen(clients[client_socket].current_channel) > 0) {
                // Exiting channel
                memset(clients[client_socket].current_channel, 0, sizeof(clients[client_socket].current_channel));
                send_response(client_socket, "Exited channel\n");
            } else {
                // Exiting client
                send_response(client_socket, "Exited\n");
                break;
            }
        } else if (strcmp(command, "REMOVE") == 0) {
            char *sub_command = strtok(NULL, " ");

            if (strcmp(sub_command, "USER") == 0) {
                char *user_to_remove = strtok(NULL, " ");
                remove_user_from_channel(clients[client_socket].username, clients[client_socket].current_channel, user_to_remove, client_socket);
            } else {
                remove_user(sub_command, client_socket);
            }
        } else if (strcmp(command, "BAN") == 0) {
            char *user_to_ban = strtok(NULL, " ");
            ban_user(clients[client_socket].username, clients[client_socket].current_channel, user_to_ban, client_socket);
        } else if (strcmp(command, "UNBAN") == 0) {
            char *user_to_unban = strtok(NULL, " ");
            unban_user(clients[client_socket].username, clients[client_socket].current_channel, user_to_unban, client_socket);
        } else {
            send_response(client_socket, "Command tidak dikenal\n");
        }
        // Handle other commands here...
    }

    close(client_socket);
}

void *connection_handler(void *socket_desc) {
    int client_socket = *(int *)socket_desc;
    pthread_mutex_lock(&clients_mutex);
    clients[client_socket].socket = client_socket;
    clients[client_socket].role = 0; // Default role USER
    client_count++;
    pthread_mutex_unlock(&clients_mutex);

    handle_client(client_socket);

    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].socket == client_socket) {
            clients[i] = clients[client_count - 1];
            client_count--;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    free(socket_desc);
    pthread_exit(NULL);
}

int main() {
    int server_fd, client_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    pthread_t tid;

    load_users();

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 3) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    while ((client_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) >= 0) {
        int *new_sock = malloc(1);
        *new_sock = client_socket;
        if (pthread_create(&tid, NULL, connection_handler, (void *)new_sock) != 0) {
            perror("Thread creation failed");
            free(new_sock);
        }
    }

    if (client_socket < 0) {
        perror("Accept failed");
        exit(EXIT_FAILURE);
    }

    return 0;
}

void load_users() {
    FILE *file = fopen("users.csv", "r");
    if (file == NULL) {
        perror("Failed to open users.csv");
        exit(EXIT_FAILURE);
    }
    // Load users from file...
    fclose(file);
}

void register_user(const char* username, const char* password, int client_socket) {
    FILE *file = fopen("users.csv", "r+");
    if (file == NULL) {
        perror("Failed to open users.csv");
        exit(EXIT_FAILURE);
    }

    char line[256];
    int id_user = 0;
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        int current_id = atoi(token);
        if (current_id > id_user) {
            id_user = current_id;
        }
        token = strtok(NULL, ","); // username
        if (strcmp(token, username) == 0) {
            send(client_socket, "username sudah terdaftar\n", strlen("username sudah terdaftar\n"), 0);
            fclose(file);
            return;
        }
    }

    id_user++; // Increment the user ID for the new user
    fseek(file, 0, SEEK_END);
    char encrypted_password[BCRYPT_HASHSIZE];
    char salt[BCRYPT_HASHSIZE];

    if (bcrypt_gensalt(12, salt) != 0) {
        perror("Salt generation failed");
        fclose(file);
        return;
    }

    if (bcrypt_hashpw(password, salt, encrypted_password) != 0) {
        perror("Password encryption failed");
        fclose(file);
        return;
    }

    char global_role[10] = "USER";
    if (id_user == 1) { // If this is the first user, make them ROOT
        strcpy(global_role, "ROOT");
    }

    fprintf(file, "%d,%s,%s,%s\n", id_user, username, encrypted_password, global_role);
    send(client_socket, "username berhasil register\n", strlen("username berhasil register\n"), 0);

    fclose(file);
}

void login_user(const char* username, const char* password, int client_socket) {
    FILE *file = fopen("users.csv", "r");
    if (file == NULL) {
        perror("Failed to open users.csv");
        exit(EXIT_FAILURE);
    }

    char line[256];
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        token = strtok(NULL, ","); // username
        if (strcmp(token, username) == 0) {
            token = strtok(NULL, ","); // password
            if (bcrypt_checkpw(password, token) == 0) {
                send(client_socket, "username berhasil login\n", strlen("username berhasil login\n"), 0);
                
                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i].socket == client_socket) {
                        strcpy(clients[i].username, username);
                        // Fetch the role from the file and set it here
                        char *role = strtok(NULL, ",");
                        if (strcmp(role, "ROOT") == 0) {
                            clients[i].role = 1;
                        } else {
                            clients[i].role = 0;
                        }
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);
                
                fclose(file);
                return;
            }
        }
    }

    send(client_socket, "INVALID CREDENTIALS\n", strlen("INVALID CREDENTIALS\n"), 0);
    fclose(file);
}

void create_room(const char *username, const char *channel, const char *room, int client_socket) {
    if (!validate_csv_users(username, client_socket) || !validate_csv_auth_admin(username, channel)) {
        send_response(client_socket, "Permission denied\n");
        return;
    }

    char room_dir[BUFFER_SIZE];
    snprintf(room_dir, sizeof(room_dir), "%s/%s", channel, room);
    
    if (mkdir(room_dir, 0755) == 0) {
        // Log the creation
        char log_path[BUFFER_SIZE];
        snprintf(log_path, sizeof(log_path), "%s/admin/users.log", channel);
        FILE *log = fopen(log_path, "a");
        if (log != NULL) {
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            fprintf(log, "[%02d/%02d/%04d %02d:%02d:%02d] %s membuat room %s di channel %s\n",
                tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                username, room, channel);
        fclose(log);
    }

        send(client_socket, "Room berhasil dibuat\n", strlen("Room berhasil dibuat\n"), 0);
    } else {
        perror("Failed to create room");
        send(client_socket, "Room gagal dibuat\n", strlen("Room gagal dibuat\n"), 0);
    }
}

void join_room(const char *username, const char *channel, const char *room, int client_socket) {
    // Update clients' current room
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].socket == client_socket) {
            strcpy(clients[i].current_room, room);
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);

    // Log the join in the channel's users.log
    char log_path[BUFFER_SIZE];
    snprintf(log_path, sizeof(log_path), "%s/admin/users.log", channel);
    FILE *log = fopen(log_path, "a");
    if (log != NULL) {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        fprintf(log, "[%02d/%02d/%04d %02d:%02d:%02d] %s joined room %s in channel %s\n",
                tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                username, room, channel);
        fclose(log);
    }

    // Send response to client
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "%s joined the room %s\n", username, room);
    send(client_socket, response, strlen(response), 0);

    // Debugging output
    printf("Debug: User %s joined room %s in channel %s\n", username, room, channel);
    printf("Debug: clients[%d].current_room = %s\n", client_socket, clients[client_socket].current_room);
}

void edit_room(const char *username, const char *channel, const char *old_room, const char *new_room, int client_socket) {
    if (!validate_csv_users(username, client_socket) || !validate_csv_auth_admin(username, channel)) {
        send_response(client_socket, "Permission denied\n");
        return;
    }

    char old_room_dir[BUFFER_SIZE];
    char new_room_dir[BUFFER_SIZE];
    snprintf(old_room_dir, sizeof(old_room_dir), "%s/%s", channel, old_room);
    snprintf(new_room_dir, sizeof(new_room_dir), "%s/%s", channel, new_room);

    if (rename(old_room_dir, new_room_dir) == 0) {
        // Log the rename
        char log_path[BUFFER_SIZE];
        snprintf(log_path, sizeof(log_path), "%s/admin/users.log", channel);
        FILE *log = fopen(log_path, "a");
        if (log != NULL) {
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            fprintf(log, "[%02d/%02d/%04d %02d:%02d:%02d] %s mengubah room %s menjadi %s dalam channel %s\n",
                tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                username, old_room, new_room, channel);
        fclose(log);
    }

        send(client_socket, "Room berhasil diubah\n", strlen("Room berhasil diubah\n"), 0);
    } else {
        perror("Failed to rename room");
        send(client_socket, "Room gagal diubah\n", strlen("Room gagal diubah\n"), 0);
    }
}

void delete_room(const char *username, const char *channel, const char *room, int client_socket) {
    if (!validate_csv_users(username, client_socket) || !validate_csv_auth_admin(username, channel)) {
        send_response(client_socket, "Permission denied\n");
        return;
    }

    char room_dir[BUFFER_SIZE];
    snprintf(room_dir, sizeof(room_dir), "%s/%s", channel, room);

    if (remove_directory(room_dir) == 0) {
        // Log the deletion
        char log_path[BUFFER_SIZE];
        snprintf(log_path, sizeof(log_path), "%s/admin/users.log", channel);
        FILE *log = fopen(log_path, "a");
        if (log != NULL) {
                time_t t = time(NULL);
                struct tm tm = *localtime(&t);
                fprintf(log, "[%02d/%02d/%04d %02d:%02d:%02d] %s menghapus room %s di channel %s\n",
                tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                username, room, channel);
        fclose(log);
    }

        send(client_socket, "Room berhasil dihapus\n", strlen("Room berhasil dihapus\n"), 0);
    } else {
        perror("Failed to delete room");
        send(client_socket, "Room gagal dihapus\n", strlen("Room gagal dihapus\n"), 0);
    }
}

void delete_all_rooms(const char *username, const char *channel, int client_socket) {
    struct dirent *entry;
    DIR *dp;
    char room_path[BUFFER_SIZE];
    char log_message[BUFFER_SIZE];
    int error = 0;

    snprintf(room_path, sizeof(room_path), "%s", channel);
    dp = opendir(room_path);

    if (dp == NULL) {
        perror("Failed to open channel directory");
        send_response(client_socket, "Failed to open channel directory\n");
        return;
    }

    while ((entry = readdir(dp))) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Skip "admin" directory
        if (strcmp(entry->d_name, "admin") == 0) {
            continue;
        }

        // Build the full path to the room directory
        snprintf(room_path, sizeof(room_path), "%s/%s", channel, entry->d_name);

        // Check if it's a directory
        struct stat st;
        if (stat(room_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            // Recursively delete the directory
            DIR *room_dp = opendir(room_path);
            struct dirent *room_entry;
            while ((room_entry = readdir(room_dp))) {
                if (strcmp(room_entry->d_name, ".") == 0 || strcmp(room_entry->d_name, "..") == 0) {
                    continue;
                }
                char file_path[2 * BUFFER_SIZE];
                snprintf(file_path, sizeof(file_path), "%s/%s", room_path, room_entry->d_name);
                if (remove(file_path) != 0) {
                    perror("Failed to delete file");
                    error = 1;
                }
            }
            closedir(room_dp);
            if (rmdir(room_path) != 0) {
                perror("Failed to delete directory");
                error = 1;
            }
        }
    }
    closedir(dp);

    if (error == 0) {
        snprintf(log_message, sizeof(log_message), "Semua room berhasil dihapus\n");
        send_response(client_socket, log_message);

        // Log the deletion
        char log_path[BUFFER_SIZE];
        snprintf(log_path, sizeof(log_path), "%s/admin/users.log", channel);
        FILE *log = fopen(log_path, "a");
        if (log != NULL) {
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            fprintf(log, "[%02d/%02d/%04d %02d:%02d:%02d] %s deleted all rooms in channel %s\n",
                tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                username, channel);
        fclose(log);
    }
    } else {
        send_response(client_socket, "Error deleting rooms\n");
    }
}

void list_room(const char *channel, int client_socket) {
    char channel_dir[BUFFER_SIZE];
    snprintf(channel_dir, sizeof(channel_dir), "%s", channel);

    DIR *dir = opendir(channel_dir);
    if (dir == NULL) {
        perror("Failed to open channel directory");
        send_response(client_socket, "Gagal membuka direktori channel\n");
        return;
    }

    struct dirent *entry;
    char response[BUFFER_SIZE] = "";

    while ((entry = readdir(dir)) != NULL) {
        // Check if the entry is a directory and not "admin", ".", or ".."
        if (entry->d_type == DT_DIR && strcmp(entry->d_name, "admin") != 0 && strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            strcat(response, entry->d_name);
            strcat(response, " ");
        }
    }

    closedir(dir);

    // If no rooms found, send appropriate response
    if (strlen(response) == 0) {
        send_response(client_socket, "Tidak ada room\n");
    } else {
        send_response(client_socket, response);
    }
}



void send_chat(const char *username, const char *channel, const char *room, const char *message, int client_socket) {
    char chat_file[BUFFER_SIZE];
    snprintf(chat_file, sizeof(chat_file), "%s/%s/chat.csv", channel, room);
    FILE *file = fopen(chat_file, "a+");
    if (file == NULL) {
        perror("Failed to open chat file");
        send_response(client_socket, "Gagal mengirim pesan\n");
        return;
    }

    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    int id_chat = 1;

    if (file_size > 0) {
        fseek(file, 0, SEEK_SET);
        char line[BUFFER_SIZE];
        while (fgets(line, sizeof(line), file)) {
            id_chat++;
        }
    }

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char date[20];
    strftime(date, sizeof(date), "%d/%m/%Y %H:%M:%S", &tm);

    fprintf(file, "%s,%d,%s,\"%s\"\n", date, id_chat, username, message);
    fclose(file);

    send_response(client_socket, "Chat berhasil terkirim\n");
}

void see_chat(const char *channel, const char *room, int client_socket) {
    char chat_file[BUFFER_SIZE];
    snprintf(chat_file, sizeof(chat_file), "%s/%s/chat.csv", channel, room);
    FILE *file = fopen(chat_file, "r");
    if (file == NULL) {
        perror("Failed to open chat file");
        send_response(client_socket, "Gagal melihat pesan\n");
        return;
    }

    char response[BUFFER_SIZE * 10] = "";
    char line[BUFFER_SIZE];
    char formatted_line[BUFFER_SIZE];

    while (fgets(line, sizeof(line), file)) {
        // Tokenize and reformat the line
        char *date = strtok(line, ",");
        char *id_chat = strtok(NULL, ",");
        char *sender = strtok(NULL, ",");
        char *chat = strtok(NULL, "\n");

        snprintf(formatted_line, sizeof(formatted_line), "[%s][%s][%s] %s\n", date, id_chat, sender, chat);
        strcat(response, formatted_line);
    }

    fclose(file);
    send_response(client_socket, response);

    // Debug statement to check if the room is correctly used
    printf("Debug: see_chat called for channel %s, room %s\n", channel, room);
    printf("Debug: chat_file path is %s\n", chat_file);
}


void edit_chat(const char *channel, const char *room, int id_chat, const char *new_message, int client_socket) {
    char chat_file[BUFFER_SIZE];
    snprintf(chat_file, sizeof(chat_file), "%s/%s/chat.csv", channel, room);
    FILE *file = fopen(chat_file, "r");
    if (file == NULL) {
        perror("Failed to open chat file");
        send_response(client_socket, "Gagal mengedit pesan\n");
        return;
    }

    FILE *temp = fopen("temp.csv", "w");
    if (temp == NULL) {
        perror("Failed to open temporary file");
        fclose(file);
        send_response(client_socket, "Gagal mengedit pesan\n");
        return;
    }

    char line[BUFFER_SIZE];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        char *date = strtok(line, ",");
        char *current_id_chat = strtok(NULL, ",");
        char *sender = strtok(NULL, ",");
        char *chat = strtok(NULL, "\n");

        if (atoi(current_id_chat) == id_chat) {
            found = 1;
            // Get current timestamp
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            char new_date[BUFFER_SIZE];
            snprintf(new_date, sizeof(new_date), "%02d/%02d/%04d %02d:%02d:%02d",
                     tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                     tm.tm_hour, tm.tm_min, tm.tm_sec);

            fprintf(temp, "%s,%d,%s,\"%s\"\n", new_date, id_chat, sender, new_message);
        } else {
            fprintf(temp, "%s,%s,%s,%s\n", date, current_id_chat, sender, chat);
        }
    }

    fclose(file);
    fclose(temp);

    if (found) {
        remove(chat_file);
        rename("temp.csv", chat_file);
        send_response(client_socket, "Chat berhasil diedit\n");
    } else {
        remove("temp.csv");
        send_response(client_socket, "ID chat tidak ditemukan\n");
    }
}

void delete_chat(const char *channel, const char *room, int id_chat, int client_socket) {
    char chat_file[BUFFER_SIZE];
    snprintf(chat_file, sizeof(chat_file), "%s/%s/chat.csv", channel, room);
    FILE *file = fopen(chat_file, "r");
    if (file == NULL) {
        perror("Failed to open chat file");
        send_response(client_socket, "Gagal menghapus pesan\n");
        return;
    }

    FILE *temp = fopen("temp.csv", "w");
    if (temp == NULL) {
        perror("Failed to open temporary file");
        fclose(file);
        send_response(client_socket, "Gagal menghapus pesan\n");
        return;
    }

    char line[BUFFER_SIZE];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        char *date = strtok(line, ",");
        char *current_id_chat = strtok(NULL, ",");
        char *sender = strtok(NULL, ",");
        char *chat = strtok(NULL, "\n");

        if (atoi(current_id_chat) == id_chat) {
            found = 1;
            // Skip writing this line to delete it
            continue;
        } else {
            fprintf(temp, "%s,%s,%s,%s\n", date, current_id_chat, sender, chat);
        }
    }

    fclose(file);
    fclose(temp);

    if (found) {
        remove(chat_file);
        rename("temp.csv", chat_file);
        send_response(client_socket, "Chat berhasil dihapus\n");
    } else {
        remove("temp.csv");
        send_response(client_socket, "ID chat tidak ditemukan\n");
    }
}

void create_channel(const char* username, const char* channel, const char* key, int client_socket) {
    FILE *file = fopen("channel.csv", "r+");
    if (file == NULL) {
        perror("Failed to open channel.csv");
        exit(EXIT_FAILURE);
    }

    char line[256];
    int id_channel = 0;
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        int current_id = atoi(token);
        if (current_id > id_channel) {
            id_channel = current_id;
        }
    }

    id_channel++; // Increment the channel ID for the new channel
    fseek(file, 0, SEEK_END);
    char encrypted_key[BCRYPT_HASHSIZE];
    char salt[BCRYPT_HASHSIZE];

    if (bcrypt_gensalt(12, salt) != 0) {
        perror("Salt generation failed");
        fclose(file);
        return;
    }

    if (bcrypt_hashpw(key, salt, encrypted_key) != 0) {
        perror("Key encryption failed");
        fclose(file);
        return;
    }

    printf("Debug: created encrypted key %s\n", encrypted_key); // Debugging output

    fprintf(file, "%d,%s,%s\n", id_channel, channel, encrypted_key);
    fclose(file);

    // Create channel directory and admin directory
    char channel_dir[BUFFER_SIZE];
    snprintf(channel_dir, sizeof(channel_dir), "%s", channel);
    if (mkdir(channel_dir, 0755) != 0) {
        perror("Failed to create channel directory");
        send(client_socket, "Channel gagal dibuat\n", strlen("Channel gagal dibuat\n"), 0);
        return;
    }

    char admin_dir[BUFFER_SIZE];
    snprintf(admin_dir, sizeof(admin_dir), "%s/admin", channel);
    if (mkdir(admin_dir, 0755) != 0) {
        perror("Failed to create admin directory");
        send(client_socket, "Channel gagal dibuat\n", strlen("Channel gagal dibuat\n"), 0);
        return;
    }

    // Create auth.csv file inside admin directory
    char auth_file[BUFFER_SIZE];
    snprintf(auth_file, sizeof(auth_file), "%s/admin/auth.csv", channel);
    FILE *auth = fopen(auth_file, "w");
    if (auth == NULL) {
        perror("Failed to create auth.csv");
        return;
    }

    // Fetch the user ID and name
    int id_user = 1;

    // Fetch the user ID and name
    char user_name[50];
    FILE *users_file = fopen("users.csv", "r");
    if (users_file != NULL) {
        while (fgets(line, sizeof(line), users_file)) {
            char *token = strtok(line, ",");
            int user_id = atoi(token);
            token = strtok(NULL, ",");
            strcpy(user_name, token);
            if (strcmp(user_name, username) == 0) {
                id_user = user_id;
                break;
            }
        }
        fclose(users_file);
    }
    fprintf(auth, "%d,%s,ADMIN\n", id_user, user_name);
    fclose(auth);

    // Log the creation
    char log_path[BUFFER_SIZE];
    snprintf(log_path, sizeof(log_path), "%s/admin/users.log", channel);
    FILE *log = fopen(log_path, "a");
    if (log != NULL) {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        fprintf(log, "[%02d/%02d/%04d %02d:%02d:%02d] %s membuat %s\n",
                tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                username, channel);
        fclose(log);
    }

    send(client_socket, "Channel berhasil dibuat\n", strlen("Channel berhasil dibuat\n"), 0);
}

void edit_channel(const char* username, const char* old_channel, const char* new_channel, int client_socket) {
    if (!validate_csv_users(username, client_socket) || !validate_csv_auth_admin(username, old_channel)) {
        send_response(client_socket, "Permission denied\n");
        return;
    }

    FILE *file = fopen("channel.csv", "r");
    if (file == NULL) {
        perror("Failed to open channel.csv");
        exit(EXIT_FAILURE);
    }

    FILE *temp = fopen("temp.csv", "w");
    if (temp == NULL) {
        perror("Failed to open temp.csv");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        int id_channel = atoi(token);
        char *channel_name = strtok(NULL, ",");
        char *key = strtok(NULL, ",");

        if (strcmp(channel_name, old_channel) == 0) {
            fprintf(temp, "%d,%s,%s", id_channel, new_channel, key);
            found = 1;
        } else {
            fprintf(temp, "%d,%s,%s", id_channel, channel_name, key);
        }
    }

    fclose(file);
    fclose(temp);

    if (found) {
        remove("channel.csv");
        rename("temp.csv", "channel.csv");

        // Rename the channel directory using POSIX function
        if (rename(old_channel, new_channel) != 0) {
            perror("Failed to rename channel directory");
            send(client_socket, "Channel gagal diubah\n", strlen("Channel gagal diubah\n"), 0);
            return;
        }

        // Log the rename
        char log_path[BUFFER_SIZE];
        snprintf(log_path, sizeof(log_path), "%s/admin/users.log", new_channel);
        FILE *log = fopen(log_path, "a");
        if (log != NULL) {
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            fprintf(log, "[%02d/%02d/%04d %02d:%02d:%02d] %s mengubah %s menjadi %s\n",
                    tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                    tm.tm_hour, tm.tm_min, tm.tm_sec,
                    username, old_channel, new_channel);
            fclose(log);
        }

        send(client_socket, "Channel berhasil diubah\n", strlen("Channel berhasil diubah\n"), 0);
    } else {
        remove("temp.csv");
        send(client_socket, "Channel tidak ditemukan\n", strlen("Channel tidak ditemukan\n"), 0);
    }
}

void delete_channel(const char* username, const char* channel, int client_socket) {
    if (!validate_csv_users(username, client_socket) || !validate_csv_auth_admin(username, channel)) {
        send_response(client_socket, "Permission denied\n");
        return;
    }

    FILE *file = fopen("channel.csv", "r");
    if (file == NULL) {
        perror("Failed to open channel.csv");
        exit(EXIT_FAILURE);
    }

    FILE *temp = fopen("temp.csv", "w");
    if (temp == NULL) {
        perror("Failed to open temp.csv");
        fclose(file);
        exit(EXIT_FAILURE);
    }

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        int id_channel = atoi(token);
        char *channel_name = strtok(NULL, ",");
        char *key = strtok(NULL, ",");

        if (strcmp(channel_name, channel) == 0) {
            found = 1;
        } else {
            fprintf(temp, "%d,%s,%s", id_channel, channel_name, key);
        }
    }

    fclose(file);
    fclose(temp);

    if (found) {
        remove("channel.csv");
        rename("temp.csv", "channel.csv");

        // Remove the channel directory using POSIX function
        delete_directory(channel);

        send(client_socket, "Channel berhasil dihapus\n", strlen("Channel berhasil dihapus\n"), 0);
    } else {
        remove("temp.csv");
        send(client_socket, "Channel tidak ditemukan\n", strlen("Channel tidak ditemukan\n"), 0);
    }
}

void list_users_in_channel(const char *channel, int client_socket) {
    char auth_file[BUFFER_SIZE];
    snprintf(auth_file, sizeof(auth_file), "%s/admin/auth.csv", channel);
    FILE *file = fopen(auth_file, "r");
    if (file == NULL) {
        perror("Failed to open auth.csv");
        send_response(client_socket, "Gagal membuka auth.csv\n");
        return;
    }

    char line[BUFFER_SIZE];
    char response[BUFFER_SIZE] = "";

    while (fgets(line, sizeof(line), file)) {
        char *id_user = strtok(line, ",");
        char *username = strtok(NULL, ",");
        strcat(response, username);
        strcat(response, " ");
    }

    fclose(file);

    // If no users found, send appropriate response
    if (strlen(response) == 0) {
        send_response(client_socket, "Tidak ada user di channel ini\n");
    } else {
        send_response(client_socket, response);
    }
}

void join_channel_without_key(const char *username, const char *channel, int client_socket) {
    // int auth_result = validate_csv_auth(username, channel, client_socket);
    // if (auth_result == -1) {
    //     // User is banned, do not allow to join
    //     send_response(client_socket, "anda telah diban, silahkan menghubungi admin\n");
    //     return;
    // }
     // Set the current channel for the client
    pthread_mutex_lock(&clients_mutex);
    strcpy(clients[client_socket].current_channel, channel);
    pthread_mutex_unlock(&clients_mutex);

    char log_path[BUFFER_SIZE];
    snprintf(log_path, sizeof(log_path), "%s/admin/users.log", channel);
    FILE *log = fopen(log_path, "a");
    if (log != NULL) {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);
        fprintf(log, "[%02d/%02d/%04d %02d:%02d:%02d] %s bergabung ke channel %s\n",
                tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                tm.tm_hour, tm.tm_min, tm.tm_sec,
                username, channel);
        fclose(log);
    }

    // Send response to client
    char response[BUFFER_SIZE];
    snprintf(response, sizeof(response), "%s joined the channel %s\n", username, channel);
    send(client_socket, response, strlen(response), 0);
}

void join_channel_with_key(const char *username, const char *channel, const char *key, int client_socket) {
    FILE *file = fopen("channel.csv", "r");
    if (file == NULL) {
        perror("Could not open channel.csv");
        send_response(client_socket, "Failed to open channel.csv\n");
        return;
    }

    char line[BUFFER_SIZE];
    int success = 0;
    while (fgets(line, sizeof(line), file)) {
        char *id_channel = strtok(line, ",");
        char *file_channel = strtok(NULL, ",");
        char *file_key = strtok(NULL, "\n");

        printf("Debug: checking channel %s with key %s\n", file_channel, file_key); // Debugging output

        if (strcmp(channel, file_channel) == 0) {
            printf("Debug: channel matched %s\n", file_channel); // Debugging output
            int result = bcrypt_checkpw(key, file_key);
            printf("Debug: bcrypt_checkpw result = %d\n", result); // Debugging output
            if (result == 0) {
                printf("Debug: key matched for channel %s\n", file_channel); // Debugging output
                success = 1;
                break;
            } else {
                printf("Debug: key did not match for channel %s\n", file_channel); // Debugging output
            }
        } else {
            printf("Debug: channel %s did not match %s\n", file_channel, channel); // Debugging output
        }
    }
    fclose(file);

    if (success) {
        // Write to auth.csv
        char auth_file[BUFFER_SIZE];
        snprintf(auth_file, sizeof(auth_file), "%s/admin/auth.csv", channel);
        FILE *auth = fopen(auth_file, "a");
        if (auth == NULL) {
            perror("Failed to open auth.csv");
            send_response(client_socket, "Failed to open auth.csv\n");
            return;
        }

        // Fetch the user ID and role from users.csv
        int id_user = 0;
        char user_role[BUFFER_SIZE];
        file = fopen("users.csv", "r");
        if (file == NULL) {
            perror("Could not open users.csv");
            fclose(auth);
            send_response(client_socket, "Failed to open users.csv\n");
            return;
        }

        while (fgets(line, sizeof(line), file)) {
            char *id = strtok(line, ",");
            char *user = strtok(NULL, ",");
            char *password = strtok(NULL, ",");
            char *role = strtok(NULL, "\n");

            if (strcmp(username, user) == 0) {
                id_user = atoi(id);
                strcpy(user_role, role);
                break;
            }
        }
        fclose(file);

        fprintf(auth, "%d,%s,%s\n", id_user, username, user_role);
        fclose(auth);

        join_channel_without_key(username, channel, client_socket);
    } else {
        send_response(client_socket, "Key salah\n");
    }
}

int validate_csv_auth(const char *username, const char *channel, int client_socket) {
    char auth_file[BUFFER_SIZE];
    snprintf(auth_file, sizeof(auth_file), "%s/admin/auth.csv", channel);
    FILE *file = fopen(auth_file, "r");
    if (file == NULL) {
        printf("Debug: auth file %s not found\n", auth_file); // Debugging output
        return 0; // File tidak ditemukan, berarti user bukan admin atau ROOT
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        char *id_user = strtok(line, ",");
        char *file_username = strtok(NULL, ",");
        char *file_role = strtok(NULL, "\n");

        printf("Debug: checking username %s with role %s\n", file_username, file_role); // Debugging output

        if (strcmp(username, file_username) == 0) {
            fclose(file);
            if (strcmp(file_role, "BANNED") == 0) {
                return -1; // Return -1 jika user diban
            } else {
                return 1;
            // Jika user telah berada di dalam auth.csv, user bisa masuk langsung
            }
        }
    }
    fclose(file);
    return 0; // User tidak ditemukan dalam file auth.csv, return 0
}

int validate_csv_auth_admin(const char *username, const char *channel) {
    char auth_file[BUFFER_SIZE];
    snprintf(auth_file, sizeof(auth_file), "%s/admin/auth.csv", channel);
    FILE *file = fopen(auth_file, "r");
    if (file == NULL) {
        printf("Debug: auth file %s not found\n", auth_file); // Debugging output
        return 0; // File tidak ditemukan, berarti user bukan admin
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        char *id_user = strtok(line, ",");
        char *file_username = strtok(NULL, ",");
        char *file_role = strtok(NULL, "\n");

        printf("Debug: checking username %s with role %s\n", file_username, file_role); // Debugging output

        if (strcmp(username, file_username) == 0) {
            fclose(file);
            return (strcmp(file_role, "ADMIN") == 0 || strcmp(file_role, "ROOT") == 0) ? 1 : 0;
        }
    }
    fclose(file);
    return 0; // User tidak ditemukan dalam file auth.csv, return 0
}


int validate_csv_users(const char *username, int client_socket) {
    FILE *file = fopen("users.csv", "r");
    if (file == NULL) {
        perror("Could not open users.csv");
        exit(EXIT_FAILURE);
    }

    char line[BUFFER_SIZE];
    while (fgets(line, sizeof(line), file)) {
        char *id_user = strtok(line, ",");
        char *file_username = strtok(NULL, ",");
        char *file_password = strtok(NULL, ",");
        char *file_role = strtok(NULL, "\n");

        if (strcmp(username, file_username) == 0) {
            if (strcmp(file_role, "ROOT") == 0) {
                fclose(file);
                return 1;
            }
        }
    }
    fclose(file);
}

void list_channel(int client_socket) {
    FILE *file = fopen("channel.csv", "r");
    if (file == NULL) {
        perror("Failed to open channel.csv");
        exit(EXIT_FAILURE);
    }

    char line[256];
    char response[1024] = "";
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        token = strtok(NULL, ","); // channel name
        strcat(response, token);
        strcat(response, " ");
    }

    fclose(file);
    send(client_socket, response, strlen(response), 0);
}

void send_response(int client_socket, const char *message) {
    send(client_socket, message, strlen(message), 0);
}

void list_users(int client_socket) {
    if (validate_csv_users(clients[client_socket].username, client_socket) == 1) {
        FILE *file = fopen("users.csv", "r");
        if (file == NULL) {
            perror("Failed to open users.csv");
            send_response(client_socket, "Failed to open users.csv\n");
            return;
        }

        char line[256];
        char response[1024] = "";
        while (fgets(line, sizeof(line), file)) {
            char *token = strtok(line, ",");
            token = strtok(NULL, ","); // username
            strcat(response, token);
            strcat(response, " ");
        }

        fclose(file);
        send(client_socket, response, strlen(response), 0);
    } else {
        send_response(client_socket, "Permission denied\n");
    }
}

void edit_username(const char* current_username, const char* new_username, int client_socket) {
    if (validate_csv_users(clients[client_socket].username, client_socket) == 1) {
        FILE *file = fopen("users.csv", "r");
        if (file == NULL) {
            perror("Failed to open users.csv");
            send_response(client_socket, "Failed to open users.csv\n");
            return;
        }

        FILE *temp = fopen("temp.csv", "w");
        if (temp == NULL) {
            perror("Failed to open temp.csv");
            fclose(file);
            send_response(client_socket, "Failed to open temp.csv\n");
            return;
        }

        char line[256];
        int found = 0;
        while (fgets(line, sizeof(line), file)) {
            char *token = strtok(line, ",");
            int id_user = atoi(token);
            char *username = strtok(NULL, ",");
            char *password = strtok(NULL, ",");
            char *role = strtok(NULL, "\n");

            if (strcmp(username, current_username) == 0) {
                fprintf(temp, "%d,%s,%s,%s\n", id_user, new_username, password, role);
                found = 1;
            } else {
                fprintf(temp, "%d,%s,%s,%s\n", id_user, username, password, role);
            }
        }

        fclose(file);
        fclose(temp);

        if (found) {
            remove("users.csv");
            rename("temp.csv", "users.csv");
            send_response(client_socket, "Username berhasil diubah\n");
        } else {
            remove("temp.csv");
            send_response(client_socket, "Username tidak ditemukan\n");
        }
    } else {
        send_response(client_socket, "Permission denied\n");
    }
}

void edit_password(const char* username, const char* new_password, int client_socket) {
    if (validate_csv_users(clients[client_socket].username, client_socket) == 1) {
        FILE *file = fopen("users.csv", "r");
        if (file == NULL) {
            perror("Failed to open users.csv");
            send_response(client_socket, "Failed to open users.csv\n");
            return;
        }

        FILE *temp = fopen("temp.csv", "w");
        if (temp == NULL) {
            perror("Failed to open temp.csv");
            fclose(file);
            send_response(client_socket, "Failed to open temp.csv\n");
            return;
        }

        char line[256];
        int found = 0;
        while (fgets(line, sizeof(line), file)) {
            char *token = strtok(line, ",");
            int id_user = atoi(token);
            char *user = strtok(NULL, ",");
            char *password = strtok(NULL, ",");
            char *role = strtok(NULL, "\n");

            if (strcmp(user, username) == 0) {
                char encrypted_password[BCRYPT_HASHSIZE];
                char salt[BCRYPT_HASHSIZE];

                if (bcrypt_gensalt(12, salt) != 0) {
                    perror("Salt generation failed");
                    fclose(file);
                    fclose(temp);
                    return;
                }

                if (bcrypt_hashpw(new_password, salt, encrypted_password) != 0) {
                    perror("Password encryption failed");
                    fclose(file);
                    fclose(temp);
                    return;
                }

                fprintf(temp, "%d,%s,%s,%s\n", id_user, user, encrypted_password, role);
                found = 1;
            } else {
                fprintf(temp, "%d,%s,%s,%s\n", id_user, user, password, role);
            }
        }

        fclose(file);
        fclose(temp);

        if (found) {
            remove("users.csv");
            rename("temp.csv", "users.csv");
            send_response(client_socket, "Password berhasil diubah\n");
        } else {
            remove("temp.csv");
            send_response(client_socket, "Username tidak ditemukan\n");
        }
    } else {
        send_response(client_socket, "Permission denied\n");
    }
}

void remove_user(const char* username, int client_socket) {
    if (validate_csv_users(clients[client_socket].username, client_socket) == 1) {
        FILE *file = fopen("users.csv", "r");
        if (file == NULL) {
            perror("Failed to open users.csv");
            send_response(client_socket, "Failed to open users.csv\n");
            return;
        }

        FILE *temp = fopen("temp.csv", "w");
        if (temp == NULL) {
            perror("Failed to open temp.csv");
            fclose(file);
            send_response(client_socket, "Failed to open temp.csv\n");
            return;
        }

        char line[256];
        int found = 0;
        while (fgets(line, sizeof(line), file)) {
            char *token = strtok(line, ",");
            int id_user = atoi(token);
            char *user = strtok(NULL, ",");
            char *password = strtok(NULL, ",");
            char *role = strtok(NULL, "\n");

            if (strcmp(user, username) == 0) {
                found = 1;
            } else {
                fprintf(temp, "%d,%s,%s,%s\n", id_user, user, password, role);
            }
        }

        fclose(file);
        fclose(temp);

        if (found) {
            remove("users.csv");
            rename("temp.csv", "users.csv");
            send_response(client_socket, "User berhasil dihapus\n");
        } else {
            remove("temp.csv");
            send_response(client_socket, "Username tidak ditemukan\n");
        }
    } else {
        send_response(client_socket, "Permission denied\n");
    }
}

void edit_own_username(const char* current_username, const char* new_username, int client_socket) {
    FILE *file = fopen("users.csv", "r");
    if (file == NULL) {
        perror("Failed to open users.csv");
        send_response(client_socket, "Gagal membuka file users.csv\n");
        return;
    }

    FILE *temp = fopen("temp.csv", "w");
    if (temp == NULL) {
        perror("Failed to open temp.csv");
        fclose(file);
        send_response(client_socket, "Gagal membuka file sementara\n");
        return;
    }

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        char *id_user = strtok(line, ",");
        char *username = strtok(NULL, ",");
        char *password = strtok(NULL, ",");
        char *role = strtok(NULL, "\n");

        if (strcmp(username, current_username) == 0) {
            fprintf(temp, "%s,%s,%s,%s\n", id_user, new_username, password, role);
            found = 1;
        } else {
            fprintf(temp, "%s,%s,%s,%s\n", id_user, username, password, role);
        }
    }

    fclose(file);
    fclose(temp);

    if (found) {
        remove("users.csv");
        rename("temp.csv", "users.csv");

        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "Profil diupdate menjadi %s\n", new_username);
        send_response(client_socket, response);
    } else {
        remove("temp.csv");
        send_response(client_socket, "User tidak ditemukan\n");
    }
}

void edit_own_password(const char* username, const char* new_password, int client_socket) {
    FILE *file = fopen("users.csv", "r");
    if (file == NULL) {
        perror("Failed to open users.csv");
        send_response(client_socket, "Failed to open users.csv\n");
        return;
    }

    FILE *temp = fopen("temp.csv", "w");
    if (temp == NULL) {
        perror("Failed to open temp.csv");
        fclose(file);
        send_response(client_socket, "Failed to open temp.csv\n");
        return;
    }

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        char *token = strtok(line, ",");
        int id_user = atoi(token);
        char *user = strtok(NULL, ",");
        char *password = strtok(NULL, ",");
        char *role = strtok(NULL, "\n");

        if (strcmp(user, username) == 0) {
            char encrypted_password[BCRYPT_HASHSIZE];
            char salt[BCRYPT_HASHSIZE];

            if (bcrypt_gensalt(12, salt) != 0) {
                perror("Salt generation failed");
                fclose(file);
                fclose(temp);
                return;
            }

            if (bcrypt_hashpw(new_password, salt, encrypted_password) != 0) {
                perror("Password encryption failed");
                fclose(file);
                fclose(temp);
                return;
            }

            fprintf(temp, "%d,%s,%s,%s\n", id_user, user, encrypted_password, role);
            found = 1;
        } else {
            fprintf(temp, "%d,%s,%s,%s\n", id_user, user, password, role);
        }
    }

    fclose(file);
    fclose(temp);

    if (found) {
        remove("users.csv");
        rename("temp.csv", "users.csv");
        send_response(client_socket, "Password diupdate\n");
    } else {
        remove("temp.csv");
        send_response(client_socket, "Username tidak ditemukan\n");
    }
}

void ban_user(const char *admin, const char *channel, const char *user_to_ban, int client_socket) {
    if (!(validate_csv_users(admin, client_socket) || validate_csv_auth_admin(admin, channel))) {
        send_response(client_socket, "Permission denied\n");
        return;
    }

    char auth_file[BUFFER_SIZE];
    snprintf(auth_file, sizeof(auth_file), "%s/admin/auth.csv", channel);
    FILE *file = fopen(auth_file, "r+");
    if (file == NULL) {
        perror("Failed to open auth.csv");
        send_response(client_socket, "Gagal membuka auth.csv\n");
        return;
    }

    char temp_file[BUFFER_SIZE];
    snprintf(temp_file, sizeof(temp_file), "%s/admin/temp.csv", channel);
    FILE *temp = fopen(temp_file, "w");
    if (temp == NULL) {
        perror("Failed to open temp.csv");
        fclose(file);
        send_response(client_socket, "Gagal membuka temp.csv\n");
        return;
    }

    char line[BUFFER_SIZE];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        char *id_user = strtok(line, ",");
        char *username = strtok(NULL, ",");
        char *role = strtok(NULL, "\n");

        if (strcmp(username, user_to_ban) == 0) {
            fprintf(temp, "%s,%s,BANNED\n", id_user, username);
            found = 1;
        } else {
            fprintf(temp, "%s,%s,%s\n", id_user, username, role);
        }
    }

    fclose(file);
    fclose(temp);

    if (found) {
        remove(auth_file);
        rename(temp_file, auth_file);

        // Log the ban
        char log_path[BUFFER_SIZE];
        snprintf(log_path, sizeof(log_path), "%s/admin/users.log", channel);
        FILE *log = fopen(log_path, "a");
        if (log != NULL) {
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            fprintf(log, "[%02d/%02d/%04d %02d:%02d:%02d] %s memban %s\n",
                    tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                    tm.tm_hour, tm.tm_min, tm.tm_sec,
                    admin, user_to_ban);
            fclose(log);
        }

        send_response(client_socket, "User berhasil diban\n");
    } else {
        remove(temp_file);
        send_response(client_socket, "User tidak ditemukan\n");
    }
}

void unban_user(const char *admin, const char *channel, const char *user_to_unban, int client_socket) {
    if (!(validate_csv_users(admin, client_socket) || validate_csv_auth_admin(admin, channel))) {
        send_response(client_socket, "Permission denied\n");
        return;
    }

    char auth_file[BUFFER_SIZE];
    snprintf(auth_file, sizeof(auth_file), "%s/admin/auth.csv", channel);
    FILE *file = fopen(auth_file, "r+");
    if (file == NULL) {
        perror("Failed to open auth.csv");
        send_response(client_socket, "Gagal membuka auth.csv\n");
        return;
    }

    char temp_file[BUFFER_SIZE];
    snprintf(temp_file, sizeof(temp_file), "%s/admin/temp.csv", channel);
    FILE *temp = fopen(temp_file, "w");
    if (temp == NULL) {
        perror("Failed to open temp.csv");
        fclose(file);
        send_response(client_socket, "Gagal membuka temp.csv\n");
        return;
    }

    char line[BUFFER_SIZE];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        char *id_user = strtok(line, ",");
        char *username = strtok(NULL, ",");
        char *role = strtok(NULL, "\n");

        if (strcmp(username, user_to_unban) == 0) {
            fprintf(temp, "%s,%s,USER\n", id_user, username);
            found = 1;
        } else {
            fprintf(temp, "%s,%s,%s\n", id_user, username, role);
        }
    }

    fclose(file);
    fclose(temp);

    if (found) {
        remove(auth_file);
        rename(temp_file, auth_file);

        // Log the unban
        char log_path[BUFFER_SIZE];
        snprintf(log_path, sizeof(log_path), "%s/admin/users.log", channel);
        FILE *log = fopen(log_path, "a");
        if (log != NULL) {
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            fprintf(log, "[%02d/%02d/%04d %02d:%02d:%02d] %s mengunban %s\n",
                    tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                    tm.tm_hour, tm.tm_min, tm.tm_sec,
                    admin, user_to_unban);
            fclose(log);
        }

        send_response(client_socket, "User berhasil diunban\n");
    } else {
        remove(temp_file);
        send_response(client_socket, "User tidak ditemukan\n");
    }
}

void remove_user_from_channel(const char *admin, const char *channel, const char *user_to_remove, int client_socket) {
    if (!(validate_csv_users(admin, client_socket) || validate_csv_auth_admin(admin, channel))) {
        send_response(client_socket, "Permission denied\n");
        return;
    }

    char auth_file[BUFFER_SIZE];
    snprintf(auth_file, sizeof(auth_file), "%s/admin/auth.csv", channel);
    FILE *file = fopen(auth_file, "r+");
    if (file == NULL) {
        perror("Failed to open auth.csv");
        send_response(client_socket, "Gagal membuka auth.csv\n");
        return;
    }

    char temp_file[BUFFER_SIZE];
    snprintf(temp_file, sizeof(temp_file), "%s/admin/temp.csv", channel);
    FILE *temp = fopen(temp_file, "w");
    if (temp == NULL) {
        perror("Failed to open temp.csv");
        fclose(file);
        send_response(client_socket, "Gagal membuka temp.csv\n");
        return;
    }

    char line[BUFFER_SIZE];
    int found = 0;
    while (fgets(line, sizeof(line), file)) {
        char *id_user = strtok(line, ",");
        char *username = strtok(NULL, ",");
        char *role = strtok(NULL, "\n");

        if (strcmp(username, user_to_remove) == 0) {
            found = 1;
            continue;
        } else {
            fprintf(temp, "%s,%s,%s\n", id_user, username, role);
        }
    }

    fclose(file);
    fclose(temp);

    if (found) {
        remove(auth_file);
        rename(temp_file, auth_file);

        // Log the removal
        char log_path[BUFFER_SIZE];
        snprintf(log_path, sizeof(log_path), "%s/admin/users.log", channel);
        FILE *log = fopen(log_path, "a");
        if (log != NULL) {
            time_t t = time(NULL);
            struct tm tm = *localtime(&t);
            fprintf(log, "[%02d/%02d/%04d %02d:%02d:%02d] %s menghapus %s\n",
                    tm.tm_mday, tm.tm_mon + 1, tm.tm_year + 1900,
                    tm.tm_hour, tm.tm_min, tm.tm_sec,
                    admin, user_to_remove);
            fclose(log);
        }

        send_response(client_socket, "User berhasil dihapus dari channel\n");
    } else {
        remove(temp_file);
        send_response(client_socket, "User tidak ditemukan\n");
    }
}
