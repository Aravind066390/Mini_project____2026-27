#include"servers.h"
// Initialize base directories on server startup
void init_file_system(void) {
    mkdir(BASE_DIR, 0777);
    mkdir(COMMON_DIR, 0777);
    mkdir(USERS_DIR, 0777);
    printf("[SERVER LOG] Storage system initialized.\n");
}
// Ensures user folder and data.txt exist
void ensure_user_directory(const char *username) {
    char user_path[256];
    char data_file_path[300];

    snprintf(user_path, sizeof(user_path), "%s/%s", USERS_DIR, username);
    mkdir(user_path, 0777);

    snprintf(data_file_path, sizeof(data_file_path), "%s/data.txt", user_path);
    FILE *fp = fopen(data_file_path, "a");
    if (fp) {
        fclose(fp);
    }
}
// Append pending messages for a user
void queue_user_message(const char *username, const char *message) {
    ensure_user_directory(username);

    char data_file_path[300];
    snprintf(data_file_path, sizeof(data_file_path), "%s/%s/data.txt", USERS_DIR, username);

    FILE *fp = fopen(data_file_path, "a");
    if (fp) {
        fprintf(fp, "%s\n", message);
        fclose(fp);
        printf("[SERVER LOG] Queued message for user '%s'.\n", username);
    }
}
// Reads pending messages, sends to user, then destroys data.txt content
void process_user_login(int client_fd, const char *username) {
    ensure_user_directory(username);
    char data_file_path[300];
    snprintf(data_file_path, sizeof(data_file_path), "%s/%s/data.txt", USERS_DIR, username);
    FILE *fp = fopen(data_file_path, "r");
    char send_buffer[BUFFER_SIZE] = "--- PENDING MESSAGES ---\n";
    if (fp) {
        char line[256];
        int has_data = 0;
        while (fgets(line, sizeof(line), fp)) {
            strcat(send_buffer, line);
            has_data = 1;
        }
        fclose(fp);

        if (!has_data) {
            strcpy(send_buffer, "No pending messages.\n");
        }
        // Transmit content to client socket
        send(client_fd, send_buffer, strlen(send_buffer), 0);
        // Wipe data.txt immediately after delivery
        fp = fopen(data_file_path, "w");
        if (fp) {
            fclose(fp);
            printf("[SERVER LOG] Delivered and cleared pending data for '%s'.\n", username);
        }
    }
}
// Store files directly into the 'common/' folder
void store_common_file(const char *filename, const char *content) {
    char file_path[300];
    snprintf(file_path, sizeof(file_path), "%s/%s", COMMON_DIR, filename);

    FILE *fp = fopen(file_path, "w");
    if (fp) {
        fputs(content, fp);
        fclose(fp);
        printf("[SERVER LOG] File '%s' stored in common repository.\n", filename);
    }
}
// Thread routine to process client command requests
void handle_client_connection(void *arg) {
    int client_fd = *(int *)arg;
    free(arg);
    char recv_buf[BUFFER_SIZE] = {0};
    ssize_t bytes_read = recv(client_fd, recv_buf, sizeof(recv_buf) - 1, 0);
    if (bytes_read > 0) {
        recv_buf[bytes_read] = '\0';

        char command[32], arg1[128], arg2[512];
        int parsed = sscanf(recv_buf, "%s %s %[^\n]", command, arg1, arg2);

        if (strcmp(command, "LOGIN") == 0 && parsed >= 2) {
            // Usage: LOGIN <username>
            process_user_login(client_fd, arg1);
        }
        else if (strcmp(command, "QUEUE") == 0 && parsed >= 3) {
            // Usage: QUEUE <username> <message>
            queue_user_message(arg1, arg2);
            char *ack = "Message queued successfully.\n";
            send(client_fd, ack, strlen(ack), 0);
        }
        else if (strcmp(command, "COMMON_STORE") == 0 && parsed >= 3) {
            // Usage: COMMON_STORE <filename> <content>
            store_common_file(arg1, arg2);
            char *ack = "File saved to common folder.\n";
            send(client_fd, ack, strlen(ack), 0);
        }
        else {
            char *err = "Invalid Command. Options: LOGIN <user>, QUEUE <user> <msg>, COMMON_STORE <file> <text>\n";
            send(client_fd, err, strlen(err), 0);
        }
    }
    close(client_fd);
}
