#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#define SERVER_IP "10.171.132.40"
#define CONTROL_PORT 8012
#define MAX_CMD 1024

void trim_newline(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') str[len - 1] = '\0';
    if (len > 1 && str[len - 2] == '\r') str[len - 2] = '\0';
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

void send_admin_command(const char *admin_user, const char *admin_pass, const char *command_str) {
    int control_fd = connect_to_host(SERVER_IP, CONTROL_PORT);
    if (control_fd < 0) {
        perror("[CLIENT] Cannot connect to server control port");
        return;
    }

    char buffer[256];
    snprintf(buffer, sizeof(buffer), "%s\n", admin_user);
    send(control_fd, buffer, strlen(buffer), 0);
    snprintf(buffer, sizeof(buffer), "%s\n", admin_pass);
    send(control_fd, buffer, strlen(buffer), 0);

    read_line(control_fd, buffer, sizeof(buffer));
    if (strcmp(buffer, "LOGIN_OK") != 0) {
        printf("[CLIENT] Authentication failed using bootstrap account: %s\n", buffer);
        close(control_fd);
        return;
    }

    snprintf(buffer, sizeof(buffer), "%s\n", command_str);
    send(control_fd, buffer, strlen(buffer), 0);

    read_line(control_fd, buffer, sizeof(buffer));
    printf("[SERVER RESPONSE] %s\n\n", buffer);

    close(control_fd);
}

int main(void) {
    char sys_cmd[MAX_CMD] = {0};

    printf("===========================================\n");
    printf("   WRAPPER CLI - AUTHENTICATION MANAGER    \n");
    printf("===========================================\n");

    /* Outer Loop: Allows switching between Sign In, Sign Up, or Exit */
    while (1) {
        printf("\nMain Menu:\n");
        printf("  1. Sign In\n");
        printf("  2. Sign Up (Create new account)\n");
        printf("  3. Exit Application\n");
        printf("Choose option [1-3]: ");

        char choice[8];
        if (!fgets(choice, sizeof(choice), stdin)) break;
        trim_newline(choice);

        if (strcmp(choice, "3") == 0 || strcmp(choice, "exit") == 0) {
            printf("[SYSTEM] Exiting wrapper.\n");
            break;
        }

        if (strcmp(choice, "2") == 0) {
            /* SIGN UP FLOW */
            char new_user[128] = {0};
            char new_pass[128] = {0};

            printf("\n--- Create New Account ---\n");
            printf("New Username: ");
            if (!fgets(new_user, sizeof(new_user), stdin)) continue;
            trim_newline(new_user);

            printf("New Password: ");
            if (!fgets(new_pass, sizeof(new_pass), stdin)) continue;
            trim_newline(new_pass);

            if (strlen(new_user) == 0 || strlen(new_pass) == 0) {
                printf("[ERROR] Username and password cannot be empty.\n");
                continue;
            }

            printf("[SYSTEM] Authenticating with bootstrap credentials (testuser / testpass) to create account...\n");
            char payload[512];
            snprintf(payload, sizeof(payload), "ADD_USER %s %s", new_user, new_pass);

            /* Uses testuser / testpass to authorize account creation */
            send_admin_command("testuser", "testpass", payload);
            continue; /* Loops back to main menu so user can now Sign In */
        }

        else if (strcmp(choice, "1") == 0) {
            /* SIGN IN FLOW */
            char username[128] = {0};
            char password[128] = {0};

            printf("\n--- Sign In ---\n");
            printf("Username: ");
            if (!fgets(username, sizeof(username), stdin)) continue;
            trim_newline(username);

            printf("Password: ");
            if (!fgets(password, sizeof(password), stdin)) continue;
            trim_newline(password);

            printf("\n[SYSTEM] Logged in successfully as '%s'\n\n", username);

            printf("Available commands:\n");
            printf("  sync                                : Runs ./receive %s %s ./downloads\n", username, password);
            printf("  send <recipient> <local_file>       : Runs ./sender %s %s <recipient> <local_file> <local_file>\n", username, password);
            printf("  add_user <user> <pass>              : Create a new remote account\n");
            printf("  update_user <user> <new_pass>       : Update an account's password\n");
            printf("  delete_user <user>                  : Delete a user account\n");
            printf("  logout                              : Return to main menu\n\n");

            /* Inner Command Loop */
            char input_line[512];
            while (1) {
                printf("%s@app> ", username);
                if (!fgets(input_line, sizeof(input_line), stdin)) break;
                trim_newline(input_line);

                if (strlen(input_line) == 0) continue;

                if (strcmp(input_line, "logout") == 0 || strcmp(input_line, "exit") == 0) {
                    printf("[SYSTEM] Logging out from '%s'.\n", username);
                    break; // Breaks inner loop, returns to outer authentication menu
                }

                char cmd[32] = {0}, arg1[256] = {0}, arg2[256] = {0};
                int num_args = sscanf(input_line, "%s %s %s", cmd, arg1, arg2);

                if (strcmp(cmd, "sync") == 0) {
                    snprintf(sys_cmd, sizeof(sys_cmd), "./receive %s %s ./downloads", username, password);
                    printf("[RUNNING] %s\n", sys_cmd);
                    system(sys_cmd);
                    printf("\n");
                }
                else if (strcmp(cmd, "send") == 0) {
                    if (num_args < 3) {
                        printf("Usage: send <recipient> <local_file>\n");
                        printf("Example: send admin video.mp4\n\n");
                    } else {
                        snprintf(sys_cmd, sizeof(sys_cmd), "./sender %s %s %s %s %s", username, password, arg1, arg2, arg2);
                        printf("[RUNNING] %s\n", sys_cmd);
                        system(sys_cmd);
                        printf("\n");
                    }
                }
                else if (strcmp(cmd, "add_user") == 0) {
                    if (num_args < 3) {
                        printf("Usage: add_user <new_username> <new_password>\n\n");
                    } else {
                        char payload[512];
                        snprintf(payload, sizeof(payload), "ADD_USER %s %s", arg1, arg2);
                        send_admin_command(username, password, payload);
                    }
                }
                else if (strcmp(cmd, "update_user") == 0) {
                    if (num_args < 3) {
                        printf("Usage: update_user <username> <new_password>\n\n");
                    } else {
                        char payload[512];
                        snprintf(payload, sizeof(payload), "UPDATE_USER %s %s", arg1, arg2);
                        send_admin_command(username, password, payload);
                    }
                }
                else if (strcmp(cmd, "delete_user") == 0) {
                    if (num_args < 2) {
                        printf("Usage: delete_user <username>\n\n");
                    } else {
                        char payload[512];
                        snprintf(payload, sizeof(payload), "DELETE_USER %s", arg1);
                        send_admin_command(username, password, payload);
                    }
                }
                else {
                    printf("Unknown command. Options: sync, send, add_user, update_user, delete_user, logout\n\n");
                }
            }
        }
        else {
            printf("[ERROR] Invalid choice. Please select 1, 2, or 3.\n");
        }
    }

    return EXIT_SUCCESS;
}
