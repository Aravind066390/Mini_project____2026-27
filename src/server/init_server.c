/*
 * primary_server.c
 *
 * Architecture:
 *
 *                 CONTROL PORT
 *                    :8012
 *                       |
 *                       v
 *                +-------------+
 *                | Main Server |
 *                +-------------+
 *                       |
 *                  authenticate
 *                       |
 *                  db(user,pass)  <-- Integrated with network.h
 *                       |
 *                       v
 *                create transfer
 *                     port
 *                       |
 *              +--------+--------+
 *              |                 |
 *              v                 v
 *           SENDER            RECEIVER
 *        child process     child process
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>

// Include your custom SQL library header
#include "network.h"

#define CONTROL_PORT 8012
#define START_TRANSFER 1
#define LOGIN_OK       1
#define LOGIN_FAILED   0
#define BUFFER_SIZE 4096

/* =========================================================
   DATABASE OPERATIONS USING NETWORK.H
   ========================================================= */

/**
 * Validates user credentials against the database.
 * Returns 1 for success, 0 for failure.
 */
int db_check_user(const char *username, const char *password)
{
    char query[256];
    // Construct search command/query for the engine
    snprintf(query, sizeof(query), "SELECT * FROM users WHERE username='%s' AND password='%s';", username, password);
    
    sqlr *res = ask_sql(query);
    if (res == NULL) {
        return 0;
    }

    // Check if matching records were returned (excluding header lines if present)
    int count = res->l;
    
    // Cleanup allocated result
    delete res;

    return (count > 0) ? LOGIN_OK : LOGIN_FAILED;
}

/**
 * Adds a new user record.
 */
int db_add_user(const char *username, const char *password)
{
    char query[256];
    snprintf(query, sizeof(query), "INSERT INTO users VALUES ('%s', '%s');", username, password);
    return give_sql(query);
}

/**
 * Updates an existing user's password.
 */
int db_update_user(const char *username, const char *new_password)
{
    char query[256];
    snprintf(query, sizeof(query), "UPDATE users SET password='%s' WHERE username='%s';", new_password, username);
    return give_sql(query);
}

/**
 * Removes a user record.
 */
int db_delete_user(const char *username)
{
    char query[256];
    snprintf(query, sizeof(query), "DELETE FROM users WHERE username='%s';", username);
    return give_sql(query);
}

/* =========================================================
   CREATE LISTENING SOCKET
   ========================================================= */
int create_server_socket(int port)
{
    int sockfd;
    int opt = 1;
    struct sockaddr_in addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        return -1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        close(sockfd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 20) < 0)
    {
        perror("listen");
        close(sockfd);
        return -1;
    }

    return sockfd;
}

/* =========================================================
   FIND A FREE TRANSFER PORT
   ========================================================= */
int create_transfer_listener(int *port_out)
{
    int sockfd;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("transfer socket");
        return -1;
    }

    int opt = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0); // Let OS pick an available port

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("transfer bind");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 10) < 0)
    {
        perror("transfer listen");
        close(sockfd);
        return -1;
    }

    if (getsockname(sockfd, (struct sockaddr *)&addr, &addr_len) < 0)
    {
        perror("getsockname");
        close(sockfd);
        return -1;
    }

    *port_out = ntohs(addr.sin_port);
    return sockfd;
}

/* =========================================================
   SENDER PROCESS
   ========================================================= */
void sender_process(int transfer_port, const char *username)
{
    printf("[SENDER] PID = %d\n", getpid());
    printf("[SENDER] User      : %s\n", username);
    printf("[SENDER] Port      : %d\n", transfer_port);
    printf("[SENDER] Ready to send files...\n");

    sleep(2);
    printf("[SENDER] Finished.\n");
    exit(EXIT_SUCCESS);
}

/* =========================================================
   RECEIVER PROCESS
   ========================================================= */
void receiver_process(int transfer_listener, const char *username)
{
    printf("[RECEIVER] PID = %d\n", getpid());
    printf("[RECEIVER] User = %s\n", username);

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(transfer_listener, (struct sockaddr *)&client_addr, &client_len);

    if (client_fd < 0)
    {
        perror("[RECEIVER] accept");
        close(transfer_listener);
        exit(EXIT_FAILURE);
    }
    printf("[RECEIVER] Sender connected.\n");

    char buffer[BUFFER_SIZE];
    ssize_t n;
    while ((n = recv(client_fd, buffer, sizeof(buffer), 0)) > 0)
    {
        printf("[RECEIVER] Received %zd bytes\n", n);
    }

    if (n < 0)
    {
        perror("[RECEIVER] recv");
    }

    printf("[RECEIVER] Transfer finished.\n");
    close(client_fd);
    close(transfer_listener);
    exit(EXIT_SUCCESS);
}

/* =========================================================
   HANDLE ONE CLIENT
   ========================================================= */
void handle_client(int client_fd)
{
    char username[128];
    char password[128];

    memset(username, 0, sizeof(username));
    memset(password, 0, sizeof(password));

    /* Receive username */
    ssize_t n = recv(client_fd, username, sizeof(username) - 1, 0);
    if (n <= 0)
    {
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    username[strcspn(username, "\r\n")] = '\0';

    /* Receive password */
    n = recv(client_fd, password, sizeof(password) - 1, 0);
    if (n <= 0)
    {
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    password[strcspn(password, "\r\n")] = '\0';

    printf("\n[SERVER] Login request\n");
    printf("[SERVER] User: %s\n", username);

    /* -----------------------------------------------------
       DATABASE AUTHENTICATION (USING NETWORK.H INTERFACE)
       ----------------------------------------------------- */
    int result = db_check_user(username, password);

    if (result != LOGIN_OK)
    {
        printf("[SERVER] Login failed for %s\n", username);
        const char *response = "LOGIN_FAILED\n";
        send(client_fd, response, strlen(response), 0);
        close(client_fd);
        exit(EXIT_SUCCESS);
    }

    printf("[SERVER] Login successful for %s\n", username);
    const char *response = "LOGIN_OK\n";
    send(client_fd, response, strlen(response), 0);

    /* -----------------------------------------------------
       CREATE TRANSFER SESSION
       ----------------------------------------------------- */
    int transfer_port;
    int transfer_listener = create_transfer_listener(&transfer_port);
    if (transfer_listener < 0)
    {
        const char *error = "TRANSFER_CREATION_FAILED\n";
        send(client_fd, error, strlen(error), 0);
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    printf("[SERVER] Transfer port created: %d\n", transfer_port);

    char message[128];
    snprintf(message, sizeof(message), "TRANSFER_PORT %d\n", transfer_port);
    send(client_fd, message, strlen(message), 0);

    /* Fork Sender Process */
    pid_t sender_pid = fork();
    if (sender_pid < 0)
    {
        perror("fork sender");
        close(transfer_listener);
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    if (sender_pid == 0)
    {
        close(client_fd);
        sender_process(transfer_port, username);
    }

    /* Fork Receiver Process */
    pid_t receiver_pid = fork();
    if (receiver_pid < 0)
    {
        perror("fork receiver");
        kill(sender_pid, SIGTERM);
        close(transfer_listener);
        close(client_fd);
        exit(EXIT_FAILURE);
    }
    if (receiver_pid == 0)
    {
        close(client_fd);
        receiver_process(transfer_listener, username);
    }

    close(transfer_listener);
    printf("[SERVER] Sender PID   = %d\n", sender_pid);
    printf("[SERVER] Receiver PID = %d\n", receiver_pid);

    waitpid(sender_pid, NULL, 0);
    waitpid(receiver_pid, NULL, 0);

    printf("[SERVER] Transfer session finished.\n");
    close(client_fd);
    exit(EXIT_SUCCESS);
}

/* =========================================================
   MAIN SERVER
   ========================================================= */
int main(void)
{
    signal(SIGCHLD, SIG_IGN);

    /* Initialize Database IPC Shared Memory Space */
    start_sql((char *)"sqlite3 /var/db/app.db"); // Adjust command/path to match your SQL CLI tool

    int server_fd = create_server_socket(CONTROL_PORT);
    if (server_fd < 0)
    {
        end_sql();
        return EXIT_FAILURE;
    }

    printf("=====================================\n");
    printf("        PRIMARY SERVER STARTED\n");
    printf("=====================================\n");
    printf("[SERVER] Control port: %d\n", CONTROL_PORT);
    printf("[SERVER] Waiting for clients...\n");

    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0)
        {
            if (errno == EINTR)
                continue;
            perror("accept");
            continue;
        }

        printf("\n[SERVER] New client connected.\n");

        pid_t pid = fork();
        if (pid < 0)
        {
            perror("fork");
            close(client_fd);
            continue;
        }
        if (pid == 0)
        {
            close(server_fd);
            handle_client(client_fd);
        }

        close(client_fd);
    }

    close(server_fd);
    end_sql(); // Tear down shared memory allocations
    return EXIT_SUCCESS;
}
