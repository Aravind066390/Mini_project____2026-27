#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>

#define BUFFER_SIZE 8192

void trim_newline(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ')) {
        str[--len] = '\0';
    }
}

int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "[RUN_SERVER_GET] Usage: %s <username> <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* Lock working directory to binary location */
    char exe_path[1024];
    ssize_t len = readlink("/proc/self/exe", exe_path, sizeof(exe_path) - 1);
    if (len != -1) {
        exe_path[len] = '\0';
        chdir(dirname(exe_path));
    }

    char *username = argv[1];
    char *filename = argv[2];

    trim_newline(username);
    trim_newline(filename);

    mkdir("storage", 0777);
    mkdir("storage/users", 0777);

    char user_dir[256];
    snprintf(user_dir, sizeof(user_dir), "storage/users/%s", username);
    mkdir(user_dir, 0777);

    char filepath[512];
    snprintf(filepath, sizeof(filepath), "storage/users/%s/%s", username, filename);

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        perror("[RUN_SERVER_GET] fopen failed");
        return EXIT_FAILURE;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    /* Read raw binary payload until client closes socket (EOF) */
    while ((bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
        size_t written = fwrite(buffer, 1, bytes_read, fp);
        if (written < (size_t)bytes_read) {
            perror("[RUN_SERVER_GET] Disk write error");
            fclose(fp);
            return EXIT_FAILURE;
        }
    }

    fflush(fp);
    fclose(fp);
    return EXIT_SUCCESS;
}
