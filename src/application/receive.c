#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>

#define SERVER_IP "10.0.2.15"  /* Matches your server IP */
#define CONTROL_PORT 8012
#define BUFFER_SIZE 8192

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

int ensure_directory_exists(const char *dir_path) {
    struct stat st;
    if (stat(dir_path, &st) != 0) {
        if (mkdir(dir_path, 0755) != 0 && errno != EEXIST) {
            return -1;
        }
    }
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <username> <password> <output_dir_or_file>\n", argv[0]);
        printf("Example: %s testuser testpass ./downloads\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *user = argv[1];
    const char *pass = argv[2];
    const char *output_target = argv[3];

    /* 1. Connect to control socket */
    int control_fd = connect_to_host(SERVER_IP, CONTROL_PORT);
    if (control_fd < 0) {
        perror("[RECEIVER] Cannot connect to control port");
        return EXIT_FAILURE;
    }

    /* 2. Authenticate */
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s\n", user);
    send(control_fd, buffer, strlen(buffer), 0);
    snprintf(buffer, sizeof(buffer), "%s\n", pass);
    send(control_fd, buffer, strlen(buffer), 0);

    read_line(control_fd, buffer, sizeof(buffer));
    if (strcmp(buffer, "LOGIN_OK") != 0) {
        printf("[RECEIVER] Authentication failed: %s\n", buffer);
        close(control_fd);
        return EXIT_FAILURE;
    }

    /* 3. Issue GET command to fetch queued files */
    snprintf(buffer, sizeof(buffer), "GET\n");
    send(control_fd, buffer, strlen(buffer), 0);

    /* 4. Read assigned transfer port */
    read_line(control_fd, buffer, sizeof(buffer));
    int data_port = -1;
    if (sscanf(buffer, "TRANSFER_PORT %d", &data_port) != 1) {
        printf("[RECEIVER] Failed to get data port: %s\n", buffer);
        close(control_fd);
        return EXIT_FAILURE;
    }
    close(control_fd);

    /* 5. Connect to data socket */
    int data_fd = connect_to_host(SERVER_IP, data_port);
    if (data_fd < 0) {
        perror("[RECEIVER] Failed to connect to data port");
        return EXIT_FAILURE;
    }

    /* Ensure output folder exists if downloading to directory */
    ensure_directory_exists(output_target);

    char save_path[512];
    struct stat st;
    if (stat(output_target, &st) == 0 && S_ISDIR(st.st_mode)) {
        snprintf(save_path, sizeof(save_path), "%s/downloaded_mailbox.bin", output_target);
    } else {
        snprintf(save_path, sizeof(save_path), "%s", output_target);
    }

    FILE *fp = fopen(save_path, "wb");
    if (!fp) {
        perror("[RECEIVER] Cannot open local output destination");
        close(data_fd);
        return EXIT_FAILURE;
    }

    printf("[RECEIVER] Retrieving all pending files into '%s'...\n", save_path);
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
        printf("[RECEIVER] Mailbox empty. No pending files found on server.\n");
        unlink(save_path); // Delete empty file
    } else {
        printf("[RECEIVER] Download complete. Total retrieved: %zu bytes.\n", total_bytes);
    }

    return EXIT_SUCCESS;
}
