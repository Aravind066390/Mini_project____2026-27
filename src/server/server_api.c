#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_CMD 1024

/* Helper to strip trailing newline characters */
void trim_newline(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') str[len - 1] = '\0';
    if (len > 1 && str[len - 2] == '\r') str[len - 2] = '\0';
}

int main(void) {
    char username[128] = {0};
    char password[128] = {0};
    char sys_cmd[MAX_CMD] = {0};

    printf("===========================================\n");
    printf("   WRAPPER CLI FOR RECEIVE & SENDER        \n");
    printf("===========================================\n");

    /* 1. Prompt for Credentials Once */
    printf("Username: ");
    if (!fgets(username, sizeof(username), stdin)) return EXIT_FAILURE;
    trim_newline(username);

    printf("Password: ");
    if (!fgets(password, sizeof(password), stdin)) return EXIT_FAILURE;
    trim_newline(password);

    printf("\n[SYSTEM] Logged in as '%s'\n\n", username);

    /* 2. Interactive Command Loop */
    printf("Available commands:\n");
    printf("  sync                                : Runs ./receive %s %s ./downloads\n", username, password);
    printf("  send <recipient> <local_file>       : Runs ./sender %s %s <recipient> <local_file> <local_file>\n", username, password);
    printf("  exit                                : Quit application\n\n");

    char input_line[512];
    while (1) {
        printf("%s@app> ", username);
        if (!fgets(input_line, sizeof(input_line), stdin)) break;
        trim_newline(input_line);

        if (strlen(input_line) == 0) continue;

        if (strcmp(input_line, "exit") == 0) {
            printf("[SYSTEM] Exiting wrapper.\n");
            break;
        }

        char cmd[32] = {0}, arg1[256] = {0}, arg2[256] = {0};
        int num_args = sscanf(input_line, "%s %s %s", cmd, arg1, arg2);

        if (strcmp(cmd, "sync") == 0) {
            /*
             * Executes: ./receive <username> <password> ./downloads
             */
            snprintf(sys_cmd, sizeof(sys_cmd), "./receive %s %s ./downloads", username, password);
            printf("[RUNNING] %s\n", sys_cmd);
            system(sys_cmd);
            printf("\n");
        }
        else if (strcmp(cmd, "send") == 0) {
            if (num_args < 3) {
                printf("Usage: send <recipient> <local_file>\n");
                printf("Example: send srt video.mp4\n\n");
            } else {
                /*
                 * Keeps remote_file identical to local_file (arg2)
                 * Executes: ./sender <username> <password> <recipient> <local_file> <local_file>
                 */
                snprintf(sys_cmd, sizeof(sys_cmd), "./sender %s %s %s %s %s", username, password, arg1, arg2, arg2);
                printf("[RUNNING] %s\n", sys_cmd);
                system(sys_cmd);
                printf("\n");
            }
        }
        else {
            printf("Unknown command. Options: sync, send <recipient> <local_file>, exit\n\n");
        }
    }

    return EXIT_SUCCESS;
}
