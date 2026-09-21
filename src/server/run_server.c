#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#define BASE_DIR "storage"
#define COMMON_DIR "storage/common"
#define USERS_DIR "storage/users"
#define BUFFER_SIZE 4096
#define PORT 8080
// Initialize base directories on boot
void init_file_system(void) {
    mkdir(BASE_DIR, 0777);
    mkdir(COMMON_DIR, 0777);
    mkdir(USERS_DIR, 0777);
    printf("[SERVER LOG] Storage system initialized.\n");
}
// Upload processor: checks user folder, logs to data.txt, streams file data until socket closes
void handle_user_file_upload(int client_fd, const char *username, const char *filename) {
    char user_path[256];
    char data_txt_path[300];
    char user_file_path[300];
    // 1. Check if user-named directory exists; open if present, create if missing
    snprintf(user_path, sizeof(user_path), "%s/%s", USERS_DIR, username);
    mkdir(user_path, 0777);
    // 2. Check if data.txt exists; create if absent and append filename
    snprintf(data_txt_path, sizeof(data_txt_path), "%s/data.txt", user_path);
    FILE *data_fp = fopen(data_txt_path, "a+");
    if (data_fp) {
        fprintf(data_fp, "%s\n", filename);
        fclose(data_fp);
        printf("[SERVER LOG] Recorded filename '%s' in %s\n", filename, data_txt_path);
    } else {
        perror("Failed to update index file");
        return;
    }
    // 3. Open target file in user space
    snprintf(user_file_path, sizeof(user_file_path), "%s/%s", user_path, filename);
    FILE *file_fp = fopen(user_file_path, "wb");
    if (!file_fp) {
        perror("Failed to create file in user space");
        return;
    }
    // 4. Continuously listen/read from socket until data stream ends (recv returns 0)
    char chunk[BUFFER_SIZE];
    ssize_t bytes_read;
    printf("[SERVER LOG] Receiving file content from client socket...\n");

    while ((bytes_read = recv(client_fd, chunk, sizeof(chunk), 0)) > 0) {
        fwrite(chunk, 1, bytes_read, file_fp);
    }
    fclose(file_fp);
    if (bytes_read < 0) {
        perror("Error receiving data from socket");
    } else {
        printf("[SERVER LOG] Completed receiving '%s' for user '%s'. Stream ended.\n", filename, username);
    }
}
// Client request parsing and routing
void handle_client_connection(int client_fd) {
    char header_buf[BUFFER_SIZE] = {0};
    // Read initial metadata header: expected format "UPLOAD <username> <filename>\n"
    ssize_t bytes_read = recv(client_fd, header_buf, sizeof(header_buf) - 1, 0);
    if (bytes_read > 0) {
        header_buf[bytes_read] = '\0';
        char command[32], username[128], filename[128];
        int parsed = sscanf(header_buf, "%s %s %s", command, username, filename);
        if (strcmp(command, "UPLOAD") == 0 && parsed == 3) {
            // Acknowledge metadata and prepare for stream
            const char *ack = "READY_FOR_DATA\n";
            send(client_fd, ack, strlen(ack), 0);
            // Stream remaining file data until socket closes
            handle_user_file_upload(client_fd, username, filename);
        } else {
            const char *err = "Invalid Command. Usage: UPLOAD <username> <filename>\n";
            send(client_fd, err, strlen(err), 0);
        }
    }
    close(client_fd);
}
int main(void) {
    init_file_system();
    // Create server socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }
    // Address reuse setup
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    // Bind socket
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    // Listen for incoming connections
    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("[SERVER] Socket listener active on port %d...\n", PORT);
    // Infinite loop accepting client connections
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
        if (client_fd >= 0) {
            printf("\n[SERVER LOG] New client connected.\n");
            handle_client_connection(client_fd);
        }
    }
    close(server_fd);
    return 0;
}
