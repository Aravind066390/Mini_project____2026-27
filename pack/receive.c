#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <errno.h>

#define SERVER_IP "10.171.132.40"
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
        printf("Usage: %s <username> <password> <output_dir>\n", argv[0]);
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

    ensure_directory_exists(output_target);

    printf("[RECEIVER] Synchronizing pending files into '%s'...\n", output_target);
    int files_received = 0;

    while (1) {
        char filename[256];
        if (read_line(data_fd, filename, sizeof(filename)) <= 0) break;
        if (strcmp(filename, "EOF") == 0 || strlen(filename) == 0) break;

        char size_buf[64];
        if (read_line(data_fd, size_buf, sizeof(size_buf)) <= 0) break;
        long long file_size = atoll(size_buf);

        char save_path[512];
        snprintf(save_path, sizeof(save_path), "%s/%s", output_target, filename);

        FILE *fp = fopen(save_path, "wb");
        if (!fp) {
            perror("[RECEIVER] Failed to open local file for writing");
            continue;
        }

        long long bytes_remaining = file_size;
        char buf[BUFFER_SIZE];
        while (bytes_remaining > 0) {
            size_t to_read = (bytes_remaining < BUFFER_SIZE) ? bytes_remaining : BUFFER_SIZE;
            ssize_t n = recv(data_fd, buf, to_read, 0);
            if (n <= 0) break;
            fwrite(buf, 1, n, fp);
            bytes_remaining -= n;
        }

        fclose(fp);
        printf("[RECEIVER] Downloaded: %s (%lld bytes)\n", filename, file_size);
        files_received++;
    }

    close(data_fd);

    if (files_received == 0) {
        printf("[RECEIVER] Mailbox empty. No pending files found on server[cite: 3].\n");
    } else {
        printf("[RECEIVER] Sync complete. Successfully retrieved %d file(s).\n", files_received);
    }

    return EXIT_SUCCESS;
}
