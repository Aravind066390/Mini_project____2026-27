#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <libgen.h>
#include <errno.h>
#include <time.h>
#include "network.h"

#define BUFFER_SIZE 8192

void trim_string(char *str) {
    if (!str) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ')) {
        str[--len] = '\0';
    }
}

int main(int argc, char *argv[]) {
    if (argc < 5) {
        fprintf(stderr, "Usage: %s <sender> <recipient> <filename> <genre>\n", argv[0]);
        return EXIT_FAILURE;
    }

    char sender[128], recipient[128], filename[256], genre[64];
    strncpy(sender, argv[1], sizeof(sender) - 1);
    strncpy(recipient, argv[2], sizeof(recipient) - 1);
    strncpy(filename, argv[3], sizeof(filename) - 1);
    strncpy(genre, argv[4], sizeof(genre) - 1);

    trim_string(sender);
    trim_string(recipient);
    trim_string(filename);
    trim_string(genre);

    mkdir("storage", 0777);
    char filepath[1024];
    char stored_filename[512];

    if (strcmp(recipient, "admin") == 0) {
        mkdir("storage/admin", 0777);
        long timestamp = (long)time(NULL);
        snprintf(stored_filename, sizeof(stored_filename), "%s_%ld", sender, timestamp);
        snprintf(filepath, sizeof(filepath), "storage/admin/%s", stored_filename);
    } else {
        mkdir("storage/users", 0777);
        char user_dir[512];
        snprintf(user_dir, sizeof(user_dir), "storage/users/%s", recipient);
        mkdir(user_dir, 0777);
        snprintf(stored_filename, sizeof(stored_filename), "%s", filename);
        snprintf(filepath, sizeof(filepath), "%s/%s", user_dir, stored_filename);
    }

    /* 1. Open local file destination */
    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        perror("[RUN_SERVER_GET] fopen failed");
        return EXIT_FAILURE;
    }

    /* 2. Read the entire binary payload from STDIN_FILENO completely FIRST */
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    size_t total_written = 0;

    while ((bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
        size_t written = fwrite(buffer, 1, bytes_read, fp);
        if (written < (size_t)bytes_read) {
            perror("[RUN_SERVER_GET] Disk write error");
            fclose(fp);
            return EXIT_FAILURE;
        }
        total_written += written;
    }

    fflush(fp);
    fclose(fp);
    fprintf(stderr, "[RUN_SERVER_GET] Successfully wrote %zu bytes to '%s'\n", total_written, filepath);

    /* 3. Perform database insertion ONLY AFTER the file is fully saved and closed */
    if (strcmp(recipient, "admin") == 0) {
        start_sql((char *)"sudo -u postgres psql");
        char sql_query[1024];
        snprintf(sql_query, sizeof(sql_query),
            "-c \"INSERT INTO file_metadata (owner, file_name, original_name, genre) VALUES ('admin', '%s', '%s', '%s');\"",
            stored_filename, filename, genre);
        give_sql(sql_query);
        end_sql();
    }

    return EXIT_SUCCESS;
}
