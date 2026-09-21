#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>

#define BASE_DIR "storage"
#define USERS_DIR "storage/users"
#define BUFFER_SIZE 4096

// Ensures base directories exist
void init_file_system(void) {
    mkdir(BASE_DIR, 0777);
    mkdir(USERS_DIR, 0777);
}

// 1. Check user directory, 2. Check data.txt, 3. Log filename, 4. Store user file
void handle_user_file_upload(int client_fd, const char *username, const char *filename, const char *file_data) {
    char user_path[256];
    char data_txt_path[300];
    char user_file_path[300];

    // Step 1: Check if user-named directory exists; if yes open, else create
    snprintf(user_path, sizeof(user_path), "%s/%s", USERS_DIR, username);
    mkdir(user_path, 0777);

    // Step 2: Check if data.txt exists; if absent create it
    snprintf(data_txt_path, sizeof(data_txt_path), "%s/data.txt", user_path);
    FILE *data_fp = fopen(data_txt_path, "a+"); 
    if (data_fp) {
        // Step 3: Add the incoming file name to data.txt
        fprintf(data_fp, "%s\n", filename);
        fclose(data_fp);
        printf("[SERVER] Recorded filename '%s' in %s\n", filename, data_txt_path);
    }

    // Step 4: Store the file data in the user space (storage/users/<username>/<filename>)
    snprintf(user_file_path, sizeof(user_file_path), "%s/%s", user_path, filename);
    FILE *file_fp = fopen(user_file_path, "w");
    if (file_fp) {
        fputs(file_data, file_fp);
        fclose(file_fp);
        printf("[SERVER] Successfully stored file data in %s\n", user_file_path);

        const char *ack = "SUCCESS: File uploaded and indexed in user space.\n";
        send(client_fd, ack, strlen(ack), 0);
    } else {
        const char *err = "ERROR: Failed to save file.\n";
        send(client_fd, err, strlen(err), 0);
    }
}

// Socket command listener/router
void handle_client_connection(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);

    char recv_buf[BUFFER_SIZE] = {0};
    ssize_t bytes_read = recv(client_fd, recv_buf, sizeof(recv_buf) - 1, 0);

    if (bytes_read > 0) {
        recv_buf[bytes_read] = '\0';

        char command[32], username[128], filename[128], file_data[2048];
        
        // Expected format: UPLOAD <username> <filename> <file_content>
        int parsed = sscanf(recv_buf, "%s %s %s %[^\n]", command, username, filename, file_data);

        if (strcmp(command, "UPLOAD") == 0 && parsed >= 4) {
            handle_user_file_upload(client_fd, username, filename, file_data);
        } else {
            const char *err = "Invalid Command. Usage: UPLOAD <username> <filename> <file_data>\n";
            send(client_fd, err, strlen(err), 0);
        }
    }

    close(client_fd);
}
