#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>

#define BUFFER_SIZE 8192

/* Removes trailing newlines, carriage returns, or trailing spaces from strings */
void trim_string(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ')) {
        str[--len] = '\0';
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "[RUN_SERVER_RECV ERROR] Insufficient arguments.\n");
        fprintf(stderr, "Usage: %s <recipient_username> <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Lock working directory to the binary location */
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        chdir(dirname(exe_path));
    }

    char recipient[128];
    char filename[256];

    strncpy(recipient, argv[1], sizeof(recipient) - 1);
    recipient[sizeof(recipient) - 1] = '\0';

    strncpy(filename, argv[2], sizeof(filename) - 1);
    filename[sizeof(filename) - 1] = '\0';

    trim_string(recipient);
    trim_string(filename);

    if (strlen(recipient) == 0 || strlen(filename) == 0) {
        fprintf(stderr, "[RUN_SERVER_RECV ERROR] Recipient or filename is empty.\n");
        return EXIT_FAILURE;
    }

    /* 1. Ensure directory tree storage/users/<recipient> exists */
    mkdir("storage", 0777);
    mkdir("storage/users", 0777);

    char user_dir[512];
    snprintf(user_dir, sizeof(user_dir), "storage/users/%s", recipient);
    if (mkdir(user_dir, 0777) != 0 && errno != EEXIST) {
        perror("[RUN_SERVER_RECV ERROR] Failed to create recipient user directory");
        return EXIT_FAILURE;
    }

    /* 2. Build explicit file path: storage/users/<recipient>/<filename> */
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "storage/users/%s/%s", recipient, filename);

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        perror("[RUN_SERVER_RECV ERROR] fopen failed");
        return EXIT_FAILURE;
    }

    /* 3. Read binary payload from socket (redirected via STDIN_FILENO) and write to disk */
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    size_t total_written = 0;

    while ((bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
        size_t written = fwrite(buffer, 1, bytes_read, fp);
        if (written < (size_t)bytes_read) {
            perror("[RUN_SERVER_RECV ERROR] Disk write error");
            fclose(fp);
            return EXIT_FAILURE;
        }
        total_written += written;
    }

    fflush(fp);
    fclose(fp);

    fprintf(stderr, "[RUN_SERVER_RECV SUCCESS] Wrote %zu bytes to '%s'\n", total_written, filepath);
    return EXIT_SUCCESS;
}
