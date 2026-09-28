#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"  /* Update to your server's IP (e.g., 192.168.x.x) if remote */
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

int main(int argc, char *argv[]) {
    if (argc < 4) {
        printf("Usage: %s <username> <password> <output_file_name>\n", argv[0]);
        printf("Example: %s testuser testpass downloaded_video.mp4\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *user = argv[1];
    const char *pass = argv[2];
    const char *output_file = argv[3];

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

    /* 5. Connect to data socket and receive payload */
    int data_fd = connect_to_host(SERVER_IP, data_port);
    if (data_fd < 0) {
        perror("[RECEIVER] Failed to connect to data port");
        return EXIT_FAILURE;
    }

    FILE *fp = fopen(output_file, "wb");
    if (!fp) {
        perror("[RECEIVER] Cannot open local output file");
        close(data_fd);
        return EXIT_FAILURE;
    }

    printf("[RECEIVER] Downloading pending files into '%s'...\n", output_file);
    char buf[BUFFER_SIZE];
    ssize_t n;
    while ((n = recv(data_fd, buf, sizeof(buf), 0)) > 0) {
        fwrite(buf, 1, n, fp);
    }

    fflush(fp);
    printf("[RECEIVER] Download complete.\n");

    fclose(fp);
    close(data_fd);
    return EXIT_SUCCESS;
}
