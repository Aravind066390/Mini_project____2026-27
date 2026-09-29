#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "10.0.2.15"  /* Matches your server's IP */
#define CONTROL_PORT 8012
#define BUFFER_SIZE 8192

/* Ensures complete buffer delivery over TCP socket */
ssize_t send_all(int socket_fd, const void *buffer, size_t length) {
    size_t total_sent = 0;
    const char *ptr = (const char *)buffer;

    while (total_sent < length) {
        ssize_t sent = send(socket_fd, ptr + total_sent, length - total_sent, 0);
        if (sent <= 0) return -1;
        total_sent += sent;
    }
    return total_sent;
}

/* Reads line-by-line responses from control socket */
ssize_t read_line(int fd, char *buffer, size_t max_len) {
    size_t count = 0;
    while (count < max_len - 1) {
        char c;
        ssize_t n = recv(fd, &c, 1, 0);
        if (n <= 0) return n;
        if (c == '\n') break;
        if (c != '\r') buffer[count++] = c;
    }
    buffer[count] = '\0';
    return count;
}

/* Connects to target server host and port */
int connect_to_host(const char *ip, int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) return -1;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sockfd);
        return -1;
    }
    return sockfd;
}

/* Connects to control port 8012 and performs credential auth */
int authenticate(const char *user, const char *pass) {
    int control_fd = connect_to_host(SERVER_IP, CONTROL_PORT);
    if (control_fd < 0) {
        perror("[ERROR] Unable to connect to control server");
        return -1;
    }

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s\n", user);
    send(control_fd, buffer, strlen(buffer), 0);
    snprintf(buffer, sizeof(buffer), "%s\n", pass);
    send(control_fd, buffer, strlen(buffer), 0);

    read_line(control_fd, buffer, sizeof(buffer));
    if (strcmp(buffer, "LOGIN_OK") != 0) {
        printf("[AUTH] Authentication failed: %s\n", buffer);
        close(control_fd);
        return -1;
    }

    return control_fd;
}

/* Downloads queued files from server (receiver logic) */
int receive_queued_files(const char *user, const char *pass, const char *out_filename) {
    int control_fd = authenticate(user, pass);
    if (control_fd < 0) return -1;

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "GET\n");
    send(control_fd, buffer, strlen(buffer), 0);

    read_line(control_fd, buffer, sizeof(buffer));
    int data_port = -1;
    if (sscanf(buffer, "TRANSFER_PORT %d", &data_port) != 1) {
        printf("[RECEIVE] Failed to obtain transfer port: %s\n", buffer);
        close(control_fd);
        return -1;
    }
    close(control_fd);

    int data_fd = connect_to_host(SERVER_IP, data_port);
    if (data_fd < 0) {
        perror("[RECEIVE] Data port connection failed");
        return -1;
    }

    FILE *fp = fopen(out_filename, "wb");
    if (!fp) {
        perror("[RECEIVE] Local output file creation failed");
        close(data_fd);
        return -1;
    }

    printf("[RECEIVE] Fetching pending queued files into '%s'...\n", out_filename);
    char buf[BUFFER_SIZE];
    ssize_t n;
    size_t total_bytes = 0;
    while ((n = recv(data_fd, buf, sizeof(buf), 0)) > 0) {
        fwrite(buf, 1, n, fp);
        total_bytes += n;
    }

    fflush(fp);
    fclose(fp);
    close(data_fd);

    if (total_bytes == 0) {
        printf("[RECEIVE] No pending files on server storage.\n");
        unlink(out_filename);
    } else {
        printf("[RECEIVE] Successfully received %zu bytes into '%s'.\n", total_bytes, out_filename);
    }

    return 0;
}

/* Uploads local file to server storage (sender logic) */
int send_file_to_server(const char *user, const char *pass, const char *local_path, const char *remote_name) {
    FILE *fp = fopen(local_path, "rb");
    if (!fp) {
        perror("[SEND] Failed to open local file");
        return -1;
    }

    int control_fd = authenticate(user, pass);
    if (control_fd < 0) {
        fclose(fp);
        return -1;
    }

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "PUT %s\n", remote_name);
    send(control_fd, buffer, strlen(buffer), 0);

    read_line(control_fd, buffer, sizeof(buffer));
    int data_port = -1;
    if (sscanf(buffer, "TRANSFER_PORT %d", &data_port) != 1) {
        printf("[SEND] Failed to obtain transfer port: %s\n", buffer);
        close(control_fd);
        fclose(fp);
        return -1;
    }
    close(control_fd);

    int data_fd = connect_to_host(SERVER_IP, data_port);
    if (data_fd < 0) {
        perror("[SEND] Data port connection failed");
        fclose(fp);
        return -1;
    }

    printf("[SEND] Uploading '%s' as '%s'...\n", local_path, remote_name);
    char buf[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (send_all(data_fd, buf, n) < 0) {
            perror("[SEND] Transfer interrupted");
            break;
        }
    }

    printf("[SEND] File transfer completed successfully.\n");
    fclose(fp);
    close(data_fd);
    return 0;
}

int main(void) {
    char user[128];
    char pass[128];

    printf("==========================================\n");
    printf("   MULTI-PROCESS FILE TRANSFER CLIENT     \n");
    printf("==========================================\n");

    /* 1. Interactive login prompt */
    printf("Username: ");
    if (!fgets(user, sizeof(user), stdin)) return EXIT_FAILURE;
    user[strcspn(user, "\r\n")] = '\0';

    printf("Password: ");
    if (!fgets(pass, sizeof(pass), stdin)) return EXIT_FAILURE;
    pass[strcspn(pass, "\r\n")] = '\0';

    /* Validate login credentials once */
    int test_fd = authenticate(user, pass);
    if (test_fd < 0) {
        printf("[SYSTEM] Authentication failed or server unreachable. Exiting.\n");
        return EXIT_FAILURE;
    }
    close(test_fd);
    printf("[SYSTEM] Authentication successful!\n\n");

    /* 2. Automatic download phase on login */
    printf("--- INITIALIZING MAILBOX DOWNLOAD PHASE ---\n");
    receive_queued_files(user, pass, "inbox_download.bin");
    printf("--- DOWNLOAD PHASE COMPLETE ---\n\n");

    /* 3. Interactive Loop for Sending/Polling */
    printf("--- CLIENT INTERACTIVE LOOP ---\n");
    printf("Commands available:\n");
    printf("  send <local_file> <remote_name>  : Upload file to server\n");
    printf("  sync <output_file>               : Manually check server for new files\n");
    printf("  exit                             : Terminate application\n\n");

    char input_line[512];
    while (1) {
        printf("%s@transfer_pkg> ", user);
        if (!fgets(input_line, sizeof(input_line), stdin)) break;
        input_line[strcspn(input_line, "\r\n")] = '\0';

        if (strlen(input_line) == 0) continue;

        if (strcmp(input_line, "exit") == 0) {
            printf("[SYSTEM] Exiting client session.\n");
            break;
        }

        char cmd[32], arg1[256], arg2[256];
        int num_args = sscanf(input_line, "%s %s %s", cmd, arg1, arg2);

        if (strcmp(cmd, "send") == 0) {
            if (num_args < 3) {
                printf("Usage: send <local_file> <remote_file_name>\n");
                printf("Example: send video.mp4 target_video.mp4\n");
            } else {
                send_file_to_server(user, pass, arg1, arg2);
            }
        }
        else if (strcmp(cmd, "sync") == 0) {
            const char *outfile = (num_args >= 2) ? arg1 : "inbox_download.bin";
            receive_queued_files(user, pass, outfile);
        }
        else {
            printf("Unknown command. Options: send, sync, exit\n");
        }
    }

    return EXIT_SUCCESS;
}
