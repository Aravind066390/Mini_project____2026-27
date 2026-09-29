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
    /* Updated usage to include <recipient> */
    if (argc < 6) {
        printf("Usage: %s <username> <password> <recipient> <local_file> <remote_file_name>\n", argv[0]);
        printf("Example: %s arav mypass srt video.mp4 receiver_video.mp4\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *user = argv[1];
    const char *pass = argv[2];
    const char *recipient = argv[3];    /* Target user (e.g., srt) */
    const char *local_file = argv[4];
    const char *remote_file = argv[5];

    FILE *fp = fopen(local_file, "rb");
    if (!fp) {
        perror("[SENDER] Failed to open local file");
        return EXIT_FAILURE;
    }

    /* 1. Connect to control socket */
    int control_fd = connect_to_host(SERVER_IP, CONTROL_PORT);
    if (control_fd < 0) {
        perror("[SENDER] Cannot connect to control port");
        fclose(fp);
        return EXIT_FAILURE;
    }

    /* 2. Authenticate as sender (e.g., arav) */
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

    /* 3. Issue PUT command specifying RECIPIENT and REMOTE_FILE */
    snprintf(buffer, sizeof(buffer), "PUT %s %s\n", recipient, remote_file);
    send(control_fd, buffer, strlen(buffer), 0);

    /* 4. Get dynamic data transfer port */
    read_line(control_fd, buffer, sizeof(buffer));
    int data_port = -1;
    if (sscanf(buffer, "TRANSFER_PORT %d", &data_port) != 1) {
        printf("[SENDER] Failed to get data port: %s\n", buffer);
        close(control_fd);
        fclose(fp);
        return EXIT_FAILURE;
    }
    close(control_fd);

    /* 5. Connect to data socket and stream binary payload */
    int data_fd = connect_to_host(SERVER_IP, data_port);
    if (data_fd < 0) {
        perror("[SENDER] Failed to connect to data port");
        fclose(fp);
        return EXIT_FAILURE;
    }

    printf("[SENDER] Uploading '%s' to '%s' as '%s'...\n", local_file, recipient, remote_file);
    char buf[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (send_all(data_fd, buf, n) < 0) {
            perror("[SENDER] Upload interrupted");
            break;
        }
    }

    printf("[SENDER] Transfer complete. File delivered to '%s's mailbox!\n", recipient);
    fclose(fp);
    close(data_fd);
    return EXIT_SUCCESS;
}
