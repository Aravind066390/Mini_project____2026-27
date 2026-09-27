#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "network.h"

#define CONTROL_PORT 8012
#define LOGIN_OK       1
#define LOGIN_FAILED   0
#define USERS_DIR "storage/users"

void sanitize_input(char *str) {
    for (int i = 0; str[i]; i++) {
        if (!isalnum((unsigned char)str[i]) && str[i] != '_') {
            str[i] = '\0';
            break;
        }
    }
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

int db_check_user(const char *username, const char *password)
{
    char safe_user[128], safe_pass[128];
    strncpy(safe_user, username, sizeof(safe_user) - 1);
    strncpy(safe_pass, password, sizeof(safe_pass) - 1);
    safe_user[sizeof(safe_user) - 1] = '\0';
    safe_pass[sizeof(safe_pass) - 1] = '\0';

    sanitize_input(safe_user);
    sanitize_input(safe_pass);

    char query[512];
    snprintf(query, sizeof(query), "-c \"SELECT username FROM users WHERE username='%s' AND password='%s';\"", safe_user, safe_pass);

    sqlr *res = ask_sql(query);
    if (res == NULL) return LOGIN_FAILED;

    int count = res->l;
    delete res;

    return (count > 0) ? LOGIN_OK : LOGIN_FAILED;
}

int db_add_user(const char *username, const char *password)
{
    char safe_user[128], safe_pass[128];
    strncpy(safe_user, username, sizeof(safe_user) - 1);
    strncpy(safe_pass, password, sizeof(safe_pass) - 1);
    safe_user[sizeof(safe_user) - 1] = '\0';
    safe_pass[sizeof(safe_pass) - 1] = '\0';

    sanitize_input(safe_user);
    sanitize_input(safe_pass);

    /* Guarantee directory hierarchy creation */
    char user_dir[256];
    mkdir("storage", 0777);
    mkdir(USERS_DIR, 0777);
    snprintf(user_dir, sizeof(user_dir), "%s/%s", USERS_DIR, safe_user);
    mkdir(user_dir, 0777);

    char query[512];
    snprintf(query, sizeof(query), "-c \"INSERT INTO users (username, password) VALUES ('%s', '%s');\"", safe_user, safe_pass);
    return give_sql(query);
}

int db_update_user(const char *username, const char *new_password)
{
    char safe_user[128], safe_pass[128];
    strncpy(safe_user, username, sizeof(safe_user) - 1);
    strncpy(safe_pass, new_password, sizeof(safe_pass) - 1);
    safe_user[sizeof(safe_user) - 1] = '\0';
    safe_pass[sizeof(safe_pass) - 1] = '\0';

    sanitize_input(safe_user);
    sanitize_input(safe_pass);

    char query[512];
    snprintf(query, sizeof(query), "-c \"UPDATE users SET password='%s' WHERE username='%s';\"", safe_pass, safe_user);
    return give_sql(query);
}

int db_delete_user(const char *username)
{
    char safe_user[128];
    strncpy(safe_user, username, sizeof(safe_user) - 1);
    safe_user[sizeof(safe_user) - 1] = '\0';

    sanitize_input(safe_user);

    char query[512];
    snprintf(query, sizeof(query), "-c \"DELETE FROM users WHERE username='%s';\"", safe_user);
    return give_sql(query);
}

int create_server_socket(int port)
{
    int sockfd, opt = 1;
    struct sockaddr_in addr;

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(sockfd, 20) < 0) {
        close(sockfd);
        return -1;
    }

    return sockfd;
}

int create_transfer_listener(int *port_out)
{
    int sockfd, opt = 1;
    struct sockaddr_in addr;
    socklen_t addr_len = sizeof(addr);

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) return -1;

    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(0);

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0 || listen(sockfd, 1) < 0) {
        close(sockfd);
        return -1;
    }

    if (getsockname(sockfd, (struct sockaddr *)&addr, &addr_len) < 0) {
        close(sockfd);
        return -1;
    }

    *port_out = ntohs(addr.sin_port);
    return sockfd;
}

void handle_client(int client_fd)
{
    char username[128] = {0}, password[128] = {0}, command[256] = {0};

    if (read_line(client_fd, username, sizeof(username)) <= 0 ||
        read_line(client_fd, password, sizeof(password)) <= 0) {
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    if (db_check_user(username, password) != LOGIN_OK) {
        send(client_fd, "LOGIN_FAILED\n", 13, 0);
        close(client_fd);
        exit(EXIT_SUCCESS);
    }

    send(client_fd, "LOGIN_OK\n", 9, 0);

    if (read_line(client_fd, command, sizeof(command)) <= 0) {
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    if (strncmp(command, "ADD_USER", 8) == 0) {
        char target_user[128] = {0}, target_pass[128] = {0};
        sscanf(command + 9, "%127s %127s", target_user, target_pass);

        if (db_add_user(target_user, target_pass)) send(client_fd, "ADD_USER_SUCCESS\n", 17, 0);
        else send(client_fd, "ADD_USER_FAILED\n", 16, 0);
        close(client_fd);
        exit(EXIT_SUCCESS);
    }
    else if (strncmp(command, "UPDATE_USER", 11) == 0) {
        char target_user[128] = {0}, target_pass[128] = {0};
        sscanf(command + 12, "%127s %127s", target_user, target_pass);

        if (db_update_user(target_user, target_pass)) send(client_fd, "UPDATE_USER_SUCCESS\n", 20, 0);
        else send(client_fd, "UPDATE_USER_FAILED\n", 19, 0);
        close(client_fd);
        exit(EXIT_SUCCESS);
    }
    else if (strncmp(command, "DELETE_USER", 11) == 0) {
        char target_user[128] = {0};
        sscanf(command + 12, "%127s", target_user);

        if (db_delete_user(target_user)) send(client_fd, "DELETE_USER_SUCCESS\n", 20, 0);
        else send(client_fd, "DELETE_USER_FAILED\n", 19, 0);
        close(client_fd);
        exit(EXIT_SUCCESS);
    }

    int transfer_port;
    int transfer_listener = create_transfer_listener(&transfer_port);
    if (transfer_listener < 0) {
        send(client_fd, "TRANSFER_CREATION_FAILED\n", 25, 0);
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    char msg[128];
    snprintf(msg, sizeof(msg), "TRANSFER_PORT %d\n", transfer_port);
    send(client_fd, msg, strlen(msg), 0);

    struct sockaddr_in data_addr;
    socklen_t data_len = sizeof(data_addr);
    int data_fd = accept(transfer_listener, (struct sockaddr *)&data_addr, &data_len);
    close(transfer_listener);

    if (data_fd < 0) {
        perror("[SERVER] accept data socket");
        close(client_fd);
        exit(EXIT_FAILURE);
    }

    pid_t pid = fork();
    if (pid == 0) {
        close(client_fd);

        dup2(data_fd, STDIN_FILENO);
        dup2(data_fd, STDOUT_FILENO);
        close(data_fd);

        if (strncmp(command, "PUT", 3) == 0) {
            char *filename = command + 4;
            execl("./run_server_get", "run_server_get", username, filename, NULL);
        } else if (strncmp(command, "GET", 3) == 0) {
            execl("./run_server_send", "run_server_send", username, NULL);
        }

        perror("execl failed");
        exit(EXIT_FAILURE);
    }

    close(data_fd);
    waitpid(pid, NULL, 0);
    close(client_fd);
    exit(EXIT_SUCCESS);
}

int main(void)
{
    signal(SIGCHLD, SIG_IGN);

    /* Ensure default storage paths exist prior to accepting connections */
    mkdir("storage", 0777);
    mkdir(USERS_DIR, 0777);

    start_sql((char *)"sudo -u postgres psql");

    int server_fd = create_server_socket(CONTROL_PORT);
    if (server_fd < 0) {
        end_sql();
        return EXIT_FAILURE;
    }

    printf("[SERVER] Primary server started on port %d...\n", CONTROL_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) continue;

        if (fork() == 0) {
            close(server_fd);
            handle_client(client_fd);
        }
        close(client_fd);
    }

    close(server_fd);
    end_sql();
    return EXIT_SUCCESS;
}
