#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#define BASE_DIR "storage"
#define USERS_DIR "storage/users"

void init_file_system(void) {
    mkdir(BASE_DIR, 0777);
    mkdir(USERS_DIR, 0777);
}

// Directly processes parameters passed from main
void process_user_file_upload(const char *username, const char *filename, const char *file_data) {
    char user_path[256];
    char data_txt_path[300];
    char user_file_path[300];

    // 1. Ensure user folder exists
    snprintf(user_path, sizeof(user_path), "%s/%s", USERS_DIR, username);
    mkdir(user_path, 0777);

    // 2. Append filename to data.txt
    snprintf(data_txt_path, sizeof(data_txt_path), "%s/data.txt", user_path);
    FILE *data_fp = fopen(data_txt_path, "a+");
    if (data_fp) {
        fprintf(data_fp, "%s\n", filename);
        fclose(data_fp);
        printf("[SUCCESS] Recorded filename '%s' in %s\n", filename, data_txt_path);
    }

    // 3. Store file data in the user space
    snprintf(user_file_path, sizeof(user_file_path), "%s/%s", user_path, filename);
    FILE *file_fp = fopen(user_file_path, "w");
    if (file_fp) {
        fputs(file_data, file_fp);
        fclose(file_fp);
        printf("[SUCCESS] Stored file content in %s\n", user_file_path);
    } else {
        printf("[ERROR] Failed to save file.\n");
    }
}

// Parameters are passed directly to main here
int main(int argc, char *argv[]) {
    // Expecting 4 arguments: ./program <username> <filename> <file_data>
    if (argc < 4) {
        printf("Usage: %s <username> <filename> \"<file_data>\"\n", argv[0]);
        printf("Example: %s john notes.txt \"This is my note content\"\n", argv[0]);
        return 1;
    }

    // Extract parameters from argv
    const char *username = argv[1];
    const char *filename = argv[2];
    const char *file_data = argv[3];

    // Initialize directory structure
    init_file_system();

    // Call processing logic with extracted parameters
    process_user_file_upload(username, filename, file_data);

    return 0;
}
