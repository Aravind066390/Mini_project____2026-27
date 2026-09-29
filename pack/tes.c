#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include "network.h"

#define SERVER_IP "10.171.132.40"
#define CONTROL_PORT 8012
#define BUFFER_SIZE 8192

/* Guaranteed TCP socket write loop */
ssize_t send_all(int socket_fd, const void *buffer, size_t length) {
    size_t total_sent = 0;
    const char *ptr = (const char *)buffer;

    while (total_sent < length) {
        ssize_t sent = send(socket_fd, ptr + total_sent, length - total_sent, 0);
        if (sent <= 0) {
            return -1;
        }
        total_sent += sent;
    }
    return total_sent;
}

/* Reads line-delimited TCP input */
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

int authenticate_and_get_transfer_port(const char *user, const char *pass, const char *command) {
    int control_fd = connect_to_host(SERVER_IP, CONTROL_PORT);
    if (control_fd < 0) {
        printf("[CLIENT] Could not connect to control port %d\n", CONTROL_PORT);
        return -1;
    }

    /* 1. Send credentials */
    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s\n", user);
    send(control_fd, buffer, strlen(buffer), 0);
    snprintf(buffer, sizeof(buffer), "%s\n", pass);
    send(control_fd, buffer, strlen(buffer), 0);

    /* 2. Read login response */
    read_line(control_fd, buffer, sizeof(buffer));
    if (strcmp(buffer, "LOGIN_OK") != 0) {
        printf("[CLIENT] Login failed for user '%s': %s\n", user, buffer);
        close(control_fd);
        return -1;
    }
    printf("[CLIENT] Authentication successful for '%s'.\n", user);

    /* 3. Send action command */
    snprintf(buffer, sizeof(buffer), "%s\n", command);
    send(control_fd, buffer, strlen(buffer), 0);

    /* 4. Read response or transfer port */
    read_line(control_fd, buffer, sizeof(buffer));

    int transfer_port = -1;
    if (sscanf(buffer, "TRANSFER_PORT %d", &transfer_port) == 1) {
        printf("[CLIENT] Server assigned data transfer port: %d\n", transfer_port);
        close(control_fd);
        return transfer_port;
    }

    printf("[CLIENT] Command Response: %s\n", buffer);
    close(control_fd);
    return 0; // Command executed without dynamic data port (e.g. ADD_USER)
}

void test_put(const char *user, const char *pass, const char *local_file, const char *remote_file) {
    printf("\n--- Testing PUT (%s -> %s) ---\n", local_file, remote_file);

    char command[256];
    snprintf(command, sizeof(command), "PUT %s", remote_file);

    int data_port = authenticate_and_get_transfer_port(user, pass, command);
    if (data_port <= 0) return;

    FILE *fp = fopen(local_file, "rb");
    if (!fp) {
        perror("[CLIENT] Cannot open local source file");
        return;
    }

    int data_fd = connect_to_host(SERVER_IP, data_port);
    if (data_fd < 0) {
        perror("[CLIENT] Data socket connection failed");
        fclose(fp);
        return;
    }

    char buf[BUFFER_SIZE];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
        if (send_all(data_fd, buf, n) < 0) {
            perror("[CLIENT] File upload failed mid-stream");
            break;
        }
    }

    printf("[CLIENT] File upload successfully completed.\n");
    fclose(fp);
    close(data_fd);
}

void test_get(const char *user, const char *pass, const char *downloaded_file_name) {
    printf("\n--- Testing GET (downloading user directory contents for %s) ---\n", user);

    int data_port = authenticate_and_get_transfer_port(user, pass, "GET");
    if (data_port <= 0) return;

    int data_fd = connect_to_host(SERVER_IP, data_port);
    if (data_fd < 0) {
        perror("[CLIENT] Data socket connection failed");
        return;
    }

    FILE *fp = fopen(downloaded_file_name, "wb");
    if (!fp) {
        perror("[CLIENT] Cannot create local file to write");
        close(data_fd);
        return;
    }

    char buf[BUFFER_SIZE];
    ssize_t n;
    while ((n = recv(data_fd, buf, sizeof(buf), 0)) > 0) {
        fwrite(buf, 1, n, fp);
    }

    fflush(fp);
    printf("[CLIENT] File download successfully completed.\n");
    fclose(fp);
    close(data_fd);
}

void setup_initial_database_admin(void) {
    printf("[SETUP] Provisioning default 'admin' credentials into PostgreSQL database...\n");
    start_sql((char *)"sudo -u postgres psql");

    /* Using psql -c commands via give_sql */
    give_sql((char *)"-c \"CREATE TABLE IF NOT EXISTS users (username VARCHAR(128) PRIMARY KEY, password VARCHAR(128));\"");
    give_sql((char *)"-c \"INSERT INTO users (username, password) VALUES ('admin', 'adminpass') ON CONFLICT DO NOTHING;\"");

    end_sql();
}

int main(void) {
    /* 1. Insert default admin into PostgreSQL directly */
    setup_initial_database_admin();

    /* 2. Spawn init_server as a child process */
    printf("[SETUP] Starting server in background process...\n");
    pid_t server_pid = fork();

    if (server_pid == 0) {
        execl("./init_server", "init_server", NULL);
        perror("[SETUP] execl failed to start ./init_server");
        exit(EXIT_FAILURE);
    }

    sleep(1);

    /* 3. Run Test Suite */
    printf("\n================ RUNNING TEST SUITE ================\n");

    /* Test database user creation */
    printf("\n--- Testing ADD_USER ---\n");
    authenticate_and_get_transfer_port("admin", "adminpass", "ADD_USER testuser testpass");

    /* Test binary file upload (PUT) */
    test_put("testuser", "testpass", "ABC.mp4", "ABC.mp4");

    /* Test binary file download (GET) */
    test_get("testuser", "testpass", "dow.mp4");

    printf("\n================ TEST SUITE COMPLETED ================\n");

    /* 4. Cleanup Server Process */
    printf("[CLEANUP] Stopping background server (PID %d)...\n", server_pid);
    kill(server_pid, SIGTERM);
    waitpid(server_pid, NULL, 0);

    return 0;
}
