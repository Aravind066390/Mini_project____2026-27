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
            // 1. Send filename and file size header to receiver over socket
            dprintf(STDOUT_FILENO, "%s\n%lld\n", entry->d_name, (long long)st.st_size);

            // 2. Stream file binary payload
            FILE *fp = fopen(file_path, "rb");
            if (fp) {
                char buffer[BUFFER_SIZE];
                size_t n;
                while ((n = fread(buffer, 1, sizeof(buffer), fp)) > 0) {
                    size_t total_written = 0;
                    while (total_written < n) {
                        ssize_t sent = write(STDOUT_FILENO, buffer + total_written, n - total_written);
                        if (sent <= 0) {
                            if (sent < 0 && errno == EINTR) continue;
                            break;
                        }
                        total_written += sent;
                    }
                }
                fclose(fp);
            }

            // 3. Delete non-admin synced files immediately
            if (strcmp(username, "admin") != 0) {
                unlink(file_path);
            }
        }
    }

    closedir(dir);

    // 4. Send EOF marker so receiver knows all files have been sent
    dprintf(STDOUT_FILENO, "EOF\n");

    return EXIT_SUCCESS;
}
