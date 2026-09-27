#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define USERS_DIR "storage/users"
#define BUFFER_SIZE 8192

int user_exists(const char *username)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", USERS_DIR, username);
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
}

int main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <username> <filename>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *username = argv[1];
    const char *filename = argv[2];

    if (!user_exists(username)) {
        fprintf(stderr, "[ERROR] User '%s' storage directory does not exist.\n", username);
        return EXIT_FAILURE;
    }

    char file_path[512];
    snprintf(file_path, sizeof(file_path), "%s/%s/%s", USERS_DIR, username, filename);

    FILE *fp = fopen(file_path, "wb");
    if (!fp) {
        perror("[ERROR] fopen destination");
        return EXIT_FAILURE;
    }

    char buffer[BUFFER_SIZE];
    ssize_t n;

    /* Reads stream payload directly from standard input socket descriptor */
    while ((n = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
        size_t written = fwrite(buffer, 1, n, fp);
        if (written != (size_t)n) {
            perror("[ERROR] fwrite");
            fclose(fp);
            return EXIT_FAILURE;
        }
    }

    fclose(fp);
    return EXIT_SUCCESS;
}
