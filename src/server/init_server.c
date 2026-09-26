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
 *                 authenticate
 *                       |
 *                  db(user,pass)
 *                       |
 *                       v
 *                create transfer
 *                    port
 *                       |
 *             +---------+---------+
 *             |                   |
 *             v                   v
 *          SENDER              RECEIVER
 *       child process        child process
 *
 *
 * NOTE:
 * This is the first architectural skeleton.
 * The sender/receiver functions currently demonstrate
 * the process and port structure. Your existing file
 * transfer code can be inserted into them.
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

#define CONTROL_PORT 8012

#define START_TRANSFER 1
#define LOGIN_OK       1
#define LOGIN_FAILED   0

#define BUFFER_SIZE 4096


/* =========================================================
   YOUR DATABASE FUNCTION
   ========================================================= */

/*
 * Replace this with your actual db() function.
 *
 * Assumption:
 *
 *     return 1 -> valid login
 *     return 0 -> invalid login
 */

int db(const char *username, const char *password)
{
    /*
     * TEMPORARY TEST ONLY
     *
     * Replace this entire function with your DB code.
     */

    if (strcmp(username, "aravind") == 0 &&
        strcmp(password, "1234") == 0)
    {
        return 1;
    }

    return 0;
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

    /*
     * Allows the server to reuse the port after restart.
     */
    if (setsockopt(sockfd,
                   SOL_SOCKET,
                   SO_REUSEADDR,
                   &opt,
                   sizeof(opt)) < 0)
    {
        perror("setsockopt");
        close(sockfd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sockfd,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0)
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

    setsockopt(sockfd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;

    /*
     * Listen on all local interfaces.
     */
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /*
     * Port 0 means:
     *
     * "Kernel, choose an available port."
     */
    addr.sin_port = htons(0);

    if (bind(sockfd,
             (struct sockaddr *)&addr,
             sizeof(addr)) < 0)
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

    /*
     * Ask kernel which port it selected.
     */
    if (getsockname(sockfd,
                    (struct sockaddr *)&addr,
                    &addr_len) < 0)
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

void sender_process(int transfer_port,
                    const char *username)
{
    printf("[SENDER] PID = %d\n", getpid());

    printf("[SENDER] User      : %s\n", username);
    printf("[SENDER] Port      : %d\n", transfer_port);

    /*
     * THIS IS WHERE YOUR EXISTING SENDER CODE GOES.
     *
     * For example:
     *
     * connect_to_destination(server_ip, transfer_port);
     *
     * send_user_directory_files(...);
     *
     * Your current sender already contains most of this.
     */

    printf("[SENDER] Ready to send files...\n");

    /*
     * Temporary demonstration.
     */
    sleep(2);

    printf("[SENDER] Finished.\n");

    exit(EXIT_SUCCESS);
}


/* =========================================================
   RECEIVER PROCESS
   ========================================================= */

void receiver_process(int transfer_listener,
                      const char *username)
{
    printf("[RECEIVER] PID = %d\n", getpid());

    printf("[RECEIVER] User = %s\n", username);

    /*
     * The receiver waits for a connection on the
     * dynamically created transfer socket.
     */

    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);

    int client_fd =
        accept(transfer_listener,
               (struct sockaddr *)&client_addr,
               &client_len);

    if (client_fd < 0)
    {
        perror("[RECEIVER] accept");
        close(transfer_listener);
        exit(EXIT_FAILURE);
    }

    printf("[RECEIVER] Sender connected.\n");


    /*
     * THIS IS WHERE YOUR EXISTING RECEIVER CODE GOES.
     *
     * For example:
     *
     * recv()
     * fwrite()
     *
     * or later:
     *
     * AF_XDP / io_uring / mmap / shared buffers
     */

    char buffer[BUFFER_SIZE];

    ssize_t n;

    while ((n = recv(client_fd,
                     buffer,
                     sizeof(buffer),
                     0)) > 0)
    {
        /*
         * Temporary demonstration:
         *
         * Normally this would be:
         *
         * fwrite(buffer, 1, n, file);
         */
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


    /* -----------------------------------------------------
       Receive username
       ----------------------------------------------------- */

    ssize_t n = recv(client_fd,
                     username,
                     sizeof(username) - 1,
                     0);

    if (n <= 0)
    {
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    username[strcspn(username, "\r\n")] = '\0';


    /* -----------------------------------------------------
       Receive password
       ----------------------------------------------------- */

    n = recv(client_fd,
             password,
             sizeof(password) - 1,
             0);

    if (n <= 0)
    {
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    password[strcspn(password, "\r\n")] = '\0';


    printf("\n[SERVER] Login request\n");
    printf("[SERVER] User: %s\n", username);


    /* -----------------------------------------------------
       DATABASE AUTHENTICATION
       ----------------------------------------------------- */

    int result = db(username, password);


    if (result != LOGIN_OK)
    {
        /*
         * Login failed.
         */

        printf("[SERVER] Login failed for %s\n",
               username);

        const char *response = "LOGIN_FAILED\n";

        send(client_fd,
             response,
             strlen(response),
             0);

        close(client_fd);

        exit(EXIT_SUCCESS);
    }


    /*
     * Login successful.
     */

    printf("[SERVER] Login successful for %s\n",
           username);

    const char *response = "LOGIN_OK\n";

    send(client_fd,
         response,
         strlen(response),
         0);


    /* -----------------------------------------------------
       CREATE TRANSFER SESSION
       ----------------------------------------------------- */

    int transfer_port;

    int transfer_listener =
        create_transfer_listener(&transfer_port);

    if (transfer_listener < 0)
    {
        const char *error =
            "TRANSFER_CREATION_FAILED\n";

        send(client_fd,
             error,
             strlen(error),
             0);

        close(client_fd);

        exit(EXIT_FAILURE);
    }


    printf("[SERVER] Transfer port created: %d\n",
           transfer_port);


    /* -----------------------------------------------------
       SEND TRANSFER PORT TO CLIENT
       ----------------------------------------------------- */

    char message[128];

    snprintf(message,
             sizeof(message),
             "TRANSFER_PORT %d\n",
             transfer_port);

    send(client_fd,
         message,
         strlen(message),
         0);


    /* -----------------------------------------------------
       CREATE SENDER PROCESS
       ----------------------------------------------------- */

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
        /*
         * Child = sender
         */

        close(client_fd);

        sender_process(transfer_port,
                       username);
    }


    /* -----------------------------------------------------
       CREATE RECEIVER PROCESS
       ----------------------------------------------------- */

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
        /*
         * Child = receiver
         */

        close(client_fd);

        receiver_process(transfer_listener,
                         username);
    }


    /*
     * Parent/session process no longer needs
     * the transfer listener.
     *
     * The receiver child inherited it.
     */

    close(transfer_listener);


    printf("[SERVER] Sender PID   = %d\n",
           sender_pid);

    printf("[SERVER] Receiver PID = %d\n",
           receiver_pid);


    /* -----------------------------------------------------
       Wait for both processes
       ----------------------------------------------------- */

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

    /*
     * PRIMARY / CONTROL PORT
     *
     * This never changes.
     */

    int server_fd =
        create_server_socket(CONTROL_PORT);

    if (server_fd < 0)
    {
        return EXIT_FAILURE;
    }


    printf("=====================================\n");
    printf("       PRIMARY SERVER STARTED\n");
    printf("=====================================\n");

    printf("[SERVER] Control port: %d\n",
           CONTROL_PORT);

    printf("[SERVER] Waiting for clients...\n");


    while (1)
    {
        struct sockaddr_in client_addr;
        socklen_t client_len =
            sizeof(client_addr);

        int client_fd =
            accept(server_fd,
                   (struct sockaddr *)&client_addr,
                   &client_len);

        if (client_fd < 0)
        {
            if (errno == EINTR)
                continue;

            perror("accept");
            continue;
        }


        printf("\n[SERVER] New client connected.\n");


        /*
         * Create a session process.
         */

        pid_t pid = fork();

        if (pid < 0)
        {
            perror("fork");

            close(client_fd);
            continue;
        }


        if (pid == 0)
        {
            /*
             * Child handles this client.
             */

            close(server_fd);

            handle_client(client_fd);
        }


        /*
         * Parent keeps listening on 8012.
         */

        close(client_fd);
    }


    close(server_fd);

    return EXIT_SUCCESS;
}
