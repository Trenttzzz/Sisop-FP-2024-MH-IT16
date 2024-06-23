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
#define CHANNELS_CSV "/home/zaa/Desktop/sisop/fp-sisop/DiscorIT/channels.csv"
#define USERS_CSV "/home/zaa/Desktop/sisop/fp-sisop/DiscorIT/users.csv"
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
void list_channels(int client_socket, char* username);
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

        // Penanganan perintah channel
        if (strcmp(command, "CREATE") == 0 && strcmp(strtok(NULL, " "), "CHANNEL") == 0) {
            char *channel_name = strtok(NULL, " ");
            char *dash_k = strtok(NULL, " ");
            char *key = strtok(NULL, " ");

            if (channel_name && dash_k && key && strcmp(dash_k, "-k") == 0) {
                make_channel(channel_name, key);
                snprintf(response, sizeof(response), "Channel %s dibuat\n", channel_name);
                send(sock, response, strlen(response), 0);
            } else {
                send(sock, "Format CREATE CHANNEL salah\n", 30, 0);
            }
        } else if (strcmp(command, "EDIT") == 0 && strcmp(strtok(NULL, " "), "CHANNEL") == 0) {
            char *old_channel_name = strtok(NULL, " ");
            char *to = strtok(NULL, " ");
            char *new_channel_name = strtok(NULL, " ");

            if (old_channel_name && to && new_channel_name && strcmp(to, "TO") == 0) {
                // ... (logika untuk mengubah nama channel)
                snprintf(response, sizeof(response), "%s berhasil diubah menjadi %s\n", old_channel_name, new_channel_name);
                send(sock, response, strlen(response), 0);
            } else {
                send(sock, "Format EDIT CHANNEL salah\n", 28, 0);
            }
        } else if (strcmp(command, "DEL") == 0 && strcmp(strtok(NULL, " "), "CHANNEL") == 0) {
            char *channel_name = strtok(NULL, " ");

            if (channel_name) {
                // ... (logika untuk menghapus channel)
                snprintf(response, sizeof(response), "%s berhasil dihapus\n", channel_name);
                send(sock, response, strlen(response), 0);
            } else {
                send(sock, "Format DEL CHANNEL salah\n", 27, 0);
            }
        } else if (strcmp(command, "LIST") == 0 && strcmp(strtok(NULL, " "), "CHANNEL") == 0){
            list_channels(sock, logged_in_username);
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

// Function to read users from CSV
void read_users_from_csv() {
    FILE *file = fopen(USERS_CSV, "r");
    if (!file) {
        // File tidak ada, buat file baru dengan header
        file = fopen(USERS_CSV, "w");
        if (file) {
            fprintf(file, "id,name,password,global_role\n");
            fclose(file);
        } else {
            perror("Gagal membuat users.csv");
        }
        return;
    }

    char line[1024];
    user_count = 1; 
    while (fgets(line, sizeof(line), file)) {
        // Skip header line jika ada
        if (user_count == 0 && strcmp(line, "id,name,password,global_role\n") == 0) {
            user_count++;
            continue;
        }

        // Memastikan string tidak melebihi ukuran array
        line[strcspn(line, "\n")] = 0; 
        sscanf(line, "%d,%49[^,],%63[^,],%9s",
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
    FILE *file = fopen(USERS_CSV, "a"); // Mode append
    if (!file) {
        perror("Failed to open users.csv");
        return;
    }

    fprintf(file, "%d,%s,%s,%s\n", 
            users[user_count - 1].id, 
            users[user_count - 1].name, 
            users[user_count - 1].password, 
            users[user_count - 1].global_role);

    fclose(file);
}

// Function to read channels from CSV
void read_channels_from_csv() {
    FILE *file = fopen(CHANNELS_CSV, "r");
    if (!file) {
        // File tidak ada, buat file baru dengan header
        file = fopen(CHANNELS_CSV, "w");
        if (file) {
            fprintf(file, "id,name,key\n");
            fclose(file);
        } else {
            perror("Gagal membuat channels.csv");
        }
        return;
    }

    char line[1024];
    channel_count = 0; // Mulai dari indeks 0

    // Lewati header line jika ada
    fgets(line, sizeof(line), file);  

    while (fgets(line, sizeof(line), file)) {
        line[strcspn(line, "\n")] = 0; // Hapus karakter newline dari akhir baris
        sscanf(line, "%d,%49[^,],%63s",
               &channels[channel_count].id,
               channels[channel_count].name,
               channels[channel_count].key);
        channel_count++;
    }

    fclose(file);
}

// Function to write channels to CSV
void write_channels_to_csv() {
    FILE *file = fopen(CHANNELS_CSV, "w"); // Mode overwrite
    if (!file) {
        perror("Gagal membuka channels.csv");
        return;
    }

    fprintf(file, "id,name,key\n"); // Tulis header
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
    // Cek apakah channel sudah ada
    for (int i = 0; i < channel_count; i++) {
        if (strcmp(channels[i].name, name) == 0) {
            return; // Channel already exists
        }
    }

    // Buat direktori channel di dalam folder "DiscoritIT"
    char channel_dir[100];
    snprintf(channel_dir, sizeof(channel_dir), "./DiscorIT/%s", name);
    if (mkdir(channel_dir, 0777) == -1) {
        perror("Gagal membuat direktori channel");
        return;
    }

    // --- Tambahan untuk membuat folder admin ---
    char admin_path[200]; // Path untuk folder admin
    snprintf(admin_path, sizeof(admin_path), "%s/admin", channel_dir);
    if (mkdir(admin_path, 0777) == -1) {
        perror("Gagal membuat direktori admin");
        return;
    }

    // --- Membuat file auth.csv ---
    char auth_file_path[210]; // Path untuk file auth.csv
    snprintf(auth_file_path, sizeof(auth_file_path), "%s/auth.csv", admin_path);

    FILE *auth_file = fopen(auth_file_path, "w");
    if (auth_file == NULL) {
        perror("Gagal membuat file auth.csv");
        return;
    }
    fprintf(auth_file, "id_user,name,role\n"); // Tulis header
    fclose(auth_file);

    // --- Membuat file user.log ---
    char user_log_path[210]; // Path untuk file user.log
    snprintf(user_log_path, sizeof(user_log_path), "%s/user.log", admin_path);
    
    FILE *user_log_file = fopen(user_log_path, "w");
    if (user_log_file == NULL) {
        perror("Gagal membuat file user.log");
        return;
    }
    fclose(user_log_file);

    // Tambahkan channel baru ke array channels
    channels[channel_count].id = channel_count;
    strncpy(channels[channel_count].name, name, sizeof(channels[channel_count].name) - 1);
    channels[channel_count].name[sizeof(channels[channel_count].name) - 1] = '\0'; // Pastikan string diakhiri dengan null terminator

    char* salt = generate_salt();
    char* hashed_key = bcrypt(key, salt);
    strncpy(channels[channel_count].key, hashed_key, sizeof(channels[channel_count].key) - 1);
    channels[channel_count].key[sizeof(channels[channel_count].key) - 1] = '\0';

    channel_count++;

    // Tulis data channel ke channels.csv
    write_channels_to_csv();
}

// Function to list all channels
void list_channels(int client_socket, char *username) {
    char buffer[MAX_MESSAGE_LENGTH]; // Tidak perlu inisialisasi dengan "Channels:\n" lagi
    FILE *file = fopen(CHANNELS_CSV, "r");
    if (!file) {
        perror("Gagal membuka channels.csv");
        snprintf(buffer, sizeof(buffer), "Error: Terjadi kesalahan saat membaca daftar channel.\n");
        send(client_socket, buffer, strlen(buffer), 0);
        return;
    }

    // Format awal pesan sesuai permintaan
    snprintf(buffer, sizeof(buffer), "[%s] LIST CHANNEL\n", username);

    char line[1024];
    // Skip header line
    fgets(line, sizeof(line), file); 

    while (fgets(line, sizeof(line), file) != NULL) {
        char channel_name[50];
        sscanf(line, "%*d,%49[^,],%*s", channel_name);

        strcat(buffer, channel_name);
        strcat(buffer, " ");  // Tambahkan spasi setelah setiap nama channel
    }

    fclose(file);

    // Hapus spasi berlebih di akhir
    buffer[strcspn(buffer, "\n")] = 0;  

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
    char path[210];
    sprintf(path, "./channels/%s/user.log", channel_name);
    FILE *file = fopen(path, "a");
    if (file) {
        fprintf(file, "%s\n", activity);
        fclose(file);
    }
}
