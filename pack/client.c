#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "10.171.132.40"
#define CONTROL_PORT 8012
#define BUFFER_SIZE 8192

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

int main(int argc, char *argv[]) {
    if (argc < 6) {
        printf("Usage for normal user: %s <username> <password> <recipient_user> <local_file> <remote_file>\n", argv[0]);
        printf("Usage for admin storage: %s <username> <password> admin <local_file> <remote_file> <genre>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *user = argv[1];
    const char *pass = argv[2];
    const char *recipient = argv[3];
    const char *local_file = argv[4];
    const char *remote_file = argv[5];
    const char *genre = (strcmp(recipient, "admin") == 0 && argc >= 7) ? argv[6] : "";

    FILE *fp = fopen(local_file, "rb");
    if (!fp) {
        perror("[SENDER] Failed to open local file");
        return EXIT_FAILURE;
    }

    int control_fd = connect_to_host(SERVER_IP, CONTROL_PORT);
    if (control_fd < 0) {
        perror("[SENDER] Cannot connect to control port");
        fclose(fp);
        return EXIT_FAILURE;
    }

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s\n", user);
    send(control_fd, buffer, strlen(buffer), 0);
    snprintf(buffer, sizeof(buffer), "%s\n", pass);
    send(control_fd, buffer, strlen(buffer), 0);

    read_line(control_fd, buffer, sizeof(buffer));
    if (strcmp(buffer, "LOGIN_OK") != 0) {
        printf("[SENDER] Authentication failed: %s\n", buffer);
        close(control_fd);
        fclose(fp);
        return EXIT_FAILURE;
    }

    if (strcmp(recipient, "admin") == 0) {
        snprintf(buffer, sizeof(buffer), "PUT %s %s %s\n", recipient, remote_file, genre);
    } else {
        snprintf(buffer, sizeof(buffer), "PUT %s %s\n", recipient, remote_file);
    }
    send(control_fd, buffer, strlen(buffer), 0);

    read_line(control_fd, buffer, sizeof(buffer));
    int data_port = -1;
    if (sscanf(buffer, "TRANSFER_PORT %d", &data_port) != 1) {
        printf("[SENDER] Failed to get data port: %s\n", buffer);
        close(control_fd);
        fclose(fp);
        return EXIT_FAILURE;
    }
    close(control_fd);

    int data_fd = connect_to_host(SERVER_IP, data_port);
    if (data_fd < 0) {
        perror("[SENDER] Failed to connect to data port");
        fclose(fp);
        return EXIT_FAILURE;
    }

    printf("[SENDER] Uploading '%s' to '%s'...\n", local_file, recipient);
    char buf[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (send_all(data_fd, buf, n) < 0) {
            perror("[SENDER] Upload interrupted");
            break;
        }
    }

    printf("[SENDER] Transfer complete!\n");
    fclose(fp);
    close(data_fd);
    return EXIT_SUCCESS;
}
