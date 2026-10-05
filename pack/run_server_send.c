#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#define USERS_DIR "storage/users"
#define ADMIN_DIR "storage/admin"
#define BUFFER_SIZE 8192

int user_exists(const char *username)
{
    if (strcmp(username, "admin") == 0) return 1;
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", USERS_DIR, username);
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

int send_file_binary(const char *file_path)
{
    FILE *fp = fopen(file_path, "rb");
    if (!fp) return -1;

    char buffer[BUFFER_SIZE];
    size_t n;

    while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
        ssize_t total_written = 0;
        /* Guaranteed write loop to handle short socket writes */
        while (total_written < (ssize_t)n) {
            ssize_t sent = write(STDOUT_FILENO, buffer + total_written, n - total_written);
            if (sent <= 0) {
                if (sent < 0 && errno == EINTR) continue;
                fclose(fp);
                return -1;
            }
            total_written += sent;
        }
    }

    fclose(fp);
    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <username>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *username = argv[1];
    if (!user_exists(username)) return EXIT_FAILURE;

    char dir_path[512];
    if (strcmp(username, "admin") == 0) {
        snprintf(dir_path, sizeof(dir_path), "%s", ADMIN_DIR);
    } else {
        snprintf(dir_path, sizeof(dir_path), "%s/%s", USERS_DIR, username);
    }

    DIR *dir = opendir(dir_path);
    if (!dir) return EXIT_FAILURE;

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        char file_path[1024];
        snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(file_path, &st) == 0 && S_ISREG(st.st_mode)) {
            if (send_file_binary(file_path) == 0) {
                /* Unlike normal users where files are deleted immediately on sync,
                   admin space files are persisted and NOT deleted until explicit push-delete. */
                if (strcmp(username, "admin") != 0) {
                    unlink(file_path);
                }
            } else {
                closedir(dir);
                return EXIT_FAILURE;
            }
        }
    }

    closedir(dir);
    return EXIT_SUCCESS;
}
