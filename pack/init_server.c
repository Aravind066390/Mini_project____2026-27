#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libgen.h>
#include <time.h>

#include "network.h"

#define CONTROL_PORT 8012
#define LOGIN_OK       1
#define LOGIN_FAILED   0
#define USERS_DIR "storage/users"
#define ADMIN_DIR "storage/admin"
#define BUFFER_SIZE 8192

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
    snprintf(query, sizeof(query), "-t -A -c \"SELECT username FROM users WHERE username='%s' AND password='%s';\"", safe_user, safe_pass);

    sqlr *res = ask_sql(query);
    if (res == NULL) return LOGIN_FAILED;

    int is_valid = 0;
    if (res->l > 0 && res->A && res->A->value && res->A->value[0]) {
        if (strcmp(res->A->value[0], safe_user) == 0) {
            is_valid = 1;
        }
    }

    delete res;
    return is_valid ? LOGIN_OK : LOGIN_FAILED;
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

    char user_dir[256];
    mkdir("storage", 0777);
    mkdir(USERS_DIR, 0777);
    snprintf(user_dir, sizeof(user_dir), "%s/%s", USERS_DIR, safe_user);
    mkdir(user_dir, 0777);

    char query[512];
    snprintf(query, sizeof(query), "-c \"INSERT INTO users (username, password) VALUES ('%s', '%s') ON CONFLICT DO NOTHING;\"", safe_user, safe_pass);
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

    /* Handle Administrative User Management Commands */
    if (strncmp(command, "ADD_USER", 8) == 0) {
        char target_user[128] = {0}, target_pass[128] = {0};
        sscanf(command + 9, "%127s %127s", target_user, target_pass);

        if (db_add_user(target_user, target_pass)) send(client_fd, "ADD_USER_SUCCESS\n", 17, 0);
        else send(client_fd, "ADD_USER_FAILED\n", 16, 0);

        close(client_fd);
        exit(EXIT_SUCCESS);
    }
    else if (strncmp(command, "UPDATE_USER", 11) == 0) {
        char target_user[128] = {0}, new_pass[128] = {0};
        sscanf(command + 12, "%127s %127s", target_user, new_pass);

        char safe_user[128], safe_pass[128];
        strncpy(safe_user, target_user, sizeof(safe_user) - 1);
        strncpy(safe_pass, new_pass, sizeof(safe_pass) - 1);
        sanitize_input(safe_user);
        sanitize_input(safe_pass);

        char query[512];
        snprintf(query, sizeof(query), "-c \"UPDATE users SET password='%s' WHERE username='%s';\"", safe_pass, safe_user);

        if (give_sql(query)) send(client_fd, "UPDATE_SUCCESS\n", 15, 0);
        else send(client_fd, "UPDATE_FAILED\n", 14, 0);

        close(client_fd);
        exit(EXIT_SUCCESS);
    }
    else if (strncmp(command, "DELETE_USER", 11) == 0) {
        char target_user[128] = {0};
        sscanf(command + 12, "%127s", target_user);

        char safe_user[128];
        strncpy(safe_user, target_user, sizeof(safe_user) - 1);
        sanitize_input(safe_user);

        char query[512];
        snprintf(query, sizeof(query), "-c \"DELETE FROM users WHERE username='%s';\"", safe_user);
        give_sql(query);

        char user_dir[256];
        snprintf(user_dir, sizeof(user_dir), "%s/%s", USERS_DIR, safe_user);
        rmdir(user_dir);

        send(client_fd, "DELETE_SUCCESS\n", 15, 0);
        close(client_fd);
        exit(EXIT_SUCCESS);
    }

    /* Handle Genre Streaming Command (Restricted exclusively to Admin Space with file_metadata table) */
    if (strncmp(command, "STREAM_GENRE", 12) == 0) {
        char target_genre[64] = {0};
        sscanf(command + 13, "%63s", target_genre);

        char query[512];
        snprintf(query, sizeof(query), "-t -A -c \"SELECT file_name FROM file_metadata WHERE LOWER(genre)=LOWER('%s') AND owner='admin' LIMIT 1;\"", target_genre);
        sqlr *res = ask_sql(query);

        if (res != NULL && res->l > 0 && res->A && res->A->value && res->A->value[0]) {
            char *found_file = res->A->value[0];

            char src_path[512], user_dir[512], dest_path[512];
            snprintf(src_path, sizeof(src_path), "%s/%s", ADMIN_DIR, found_file);

            mkdir("storage", 0777);
            mkdir(USERS_DIR, 0777);
            snprintf(user_dir, sizeof(user_dir), "%s/%s", USERS_DIR, username);
            mkdir(user_dir, 0777);

            snprintf(dest_path, sizeof(dest_path), "%s/%s", user_dir, found_file);

            FILE *src = fopen(src_path, "rb");
            FILE *dst = fopen(dest_path, "wb");
            if (src && dst) {
                char dbuf[BUFFER_SIZE];
                size_t sz;
                while ((sz = fread(dbuf, 1, sizeof(dbuf), src)) > 0) {
                    fwrite(dbuf, 1, sz, dst);
                }
                send(client_fd, "STREAM_SUCCESS\n", 15, 0);
            } else {
                send(client_fd, "STREAM_FAILED_FILE_IO\n", 23, 0);
            }
            if (src) fclose(src);
            if (dst) fclose(dst);
        } else {
            send(client_fd, "STREAM_NO_MATCH\n", 17, 0);
        }
        if (res) delete res;
        close(client_fd);
        exit(EXIT_SUCCESS);
    }

    /* Handle Controlled Deletion Commands (Admin Only) */
    if (strncmp(command, "DELETE_FILE", 11) == 0) {
        char target_file[256] = {0};
        sscanf(command + 12, "%255s", target_file);

        if (strcmp(username, "admin") == 0) {
            char filepath[512];
            int deleted = 0;

            snprintf(filepath, sizeof(filepath), "%s/%s", ADMIN_DIR, target_file);
            if (unlink(filepath) == 0) {
                deleted = 1;
            } else {
                DIR *users_dir = opendir(USERS_DIR);
                if (users_dir) {
                    struct dirent *entry;
                    while ((entry = readdir(users_dir)) != NULL) {
                        if (entry->d_name[0] == '.') continue;
                        snprintf(filepath, sizeof(filepath), "%s/%s/%s", USERS_DIR, entry->d_name, target_file);
                        if (unlink(filepath) == 0) {
                            deleted = 1;
                            break;
                        }
                    }
                    closedir(users_dir);
                }
            }

            if (deleted) {
                char query[512];
                snprintf(query, sizeof(query), "-c \"DELETE FROM file_metadata WHERE file_name='%s';\"", target_file);
                give_sql(query);
                send(client_fd, "DELETE_FILE_SUCCESS\n", 21, 0);
            } else {
                send(client_fd, "DELETE_FILE_NOT_FOUND\n", 23, 0);
            }
        } else {
            send(client_fd, "UNAUTHORIZED\n", 13, 0);
        }
        close(client_fd);
        exit(EXIT_SUCCESS);
    }
    else if (strncmp(command, "DELETE_ALL_USER", 15) == 0) {
        char target_user[128] = {0};
        sscanf(command + 16, "%127s", target_user);

        if (strcmp(username, "admin") == 0) {
            char dir_path[512];
            if (strcmp(target_user, "admin") == 0) {
                snprintf(dir_path, sizeof(dir_path), "%s", ADMIN_DIR);
            } else {
                snprintf(dir_path, sizeof(dir_path), "%s/%s", USERS_DIR, target_user);
            }

            DIR *dir = opendir(dir_path);
            if (dir) {
                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL) {
                    if (entry->d_name[0] == '.') continue;
                    char file_path[1024];
                    snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);
                    unlink(file_path);
                }
                closedir(dir);

                char query[512];
                snprintf(query, sizeof(query), "-c \"DELETE FROM file_metadata WHERE owner='%s';\"", target_user);
                give_sql(query);

                send(client_fd, "DELETE_ALL_SUCCESS\n", 20, 0);
            } else {
                send(client_fd, "DIR_NOT_FOUND\n", 15, 0);
            }
        } else {
            send(client_fd, "UNAUTHORIZED\n", 13, 0);
        }
        close(client_fd);
        exit(EXIT_SUCCESS);
    }

    /* Handle File Transfer Commands (PUT / GET) */
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
            char action[32] = {0}, recipient[128] = {0}, filename[256] = {0}, genre[64] = {0};

            if (strncmp(command + 4, "admin", 5) == 0) {
                if (sscanf(command, "%31s %127s %255s %63s", action, recipient, filename, genre) >= 4) {
                    execl("./run_server_get", "run_server_get", username, recipient, filename, genre, NULL);
                } else {
                    exit(EXIT_FAILURE);
                }
            } else {
                if (sscanf(command, "%31s %127s %255s", action, recipient, filename) == 3) {
                    execl("./run_server_get", "run_server_get", username, recipient, filename, "none", NULL);
                } else {
                    exit(EXIT_FAILURE);
                }
            }
        } else if (strncmp(command, "GET", 3) == 0) {
            execl("./run_server_send", "run_server_send", username, NULL);
        }

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
    signal(SIGPIPE, SIG_IGN);
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        chdir(dirname(exe_path));
    }

    mkdir("storage", 0777);
    mkdir(USERS_DIR, 0777);
    mkdir(ADMIN_DIR, 0777);

    start_sql((char *)"sudo -u postgres psql");

    {
        char seed_q[256];
        snprintf(seed_q, sizeof(seed_q), "-t -A -c \"SELECT count(*) FROM users;\"");
        sqlr *res = ask_sql(seed_q);
        int total_users = 0;
        if (res && res->A && res->A->value && res->A->value[0]) {
            total_users = atoi(res->A->value[0]);
        }
        if (res) delete res;
        if (total_users == 0) {
            printf("[SERVER] Creating default admin account (admin / admin123)...\n");
            db_add_user("admin", "admin123");
        }
    }

    int server_fd = create_server_socket(CONTROL_PORT);
    if (server_fd < 0) {
        end_sql();
        return EXIT_FAILURE;
    }

    printf("[SERVER] Server running with Admin Space & file_metadata DB tracking on port %d...\n", CONTROL_PORT);

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
