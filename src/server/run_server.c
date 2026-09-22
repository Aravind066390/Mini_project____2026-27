#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>

#define BASE_DIR "storage"
#define USERS_DIR "storage/users"

#define PORT 8080
#define BUFFER_SIZE 8192

/* --------------------------------------------------
   Receive exactly 'size' bytes from socket
   -------------------------------------------------- */
int recv_all(int sockfd, void *buffer, size_t size)
{
    size_t received = 0;
    char *ptr = (char *)buffer;

    while (received < size)
    {
        ssize_t n = recv(sockfd,
                         ptr + received,
                         size - received,
                         0);

        if (n <= 0)
            return -1;

        received += n;
    }

    return 0;
}

/* --------------------------------------------------
   Create base storage directories
   -------------------------------------------------- */
void init_file_system(void)
{
    mkdir(BASE_DIR, 0777);
    mkdir(USERS_DIR, 0777);
}

/* --------------------------------------------------
   Receive file from client and store it
   -------------------------------------------------- */
int process_user_file_upload(
    int client_fd,
    const char *username,
    const char *filename,
    unsigned long long file_size)
{
    char user_path[256];
    char data_txt_path[300];
    char user_file_path[300];

    /* Create user directory */
    snprintf(user_path,
             sizeof(user_path),
             "%s/%s",
             USERS_DIR,
             username);

    mkdir(user_path, 0777);

    /* ------------------------------------------------
       Record filename in data.txt
       ------------------------------------------------ */
    snprintf(data_txt_path,
             sizeof(data_txt_path),
             "%s/data.txt",
             user_path);

    FILE *data_fp = fopen(data_txt_path, "a");

    if (!data_fp)
    {
        perror("data.txt");
        return -1;
    }

    fprintf(data_fp, "%s\n", filename);
    fclose(data_fp);

    /* ------------------------------------------------
       Create destination file
       ------------------------------------------------ */
    snprintf(user_file_path,
             sizeof(user_file_path),
             "%s/%s",
             user_path,
             filename);

    FILE *file_fp = fopen(user_file_path, "wb");

    if (!file_fp)
    {
        perror("file");
        return -1;
    }

    /* ------------------------------------------------
       Receive file data in chunks
       ------------------------------------------------ */
    char buffer[BUFFER_SIZE];

    unsigned long long remaining = file_size;

    while (remaining > 0)
    {
        size_t chunk_size =
            remaining > BUFFER_SIZE
                ? BUFFER_SIZE
                : (size_t)remaining;

        ssize_t n = recv(client_fd,
                          buffer,
                          chunk_size,
                          0);

        if (n <= 0)
        {
            fclose(file_fp);
            return -1;
        }

        fwrite(buffer, 1, n, file_fp);

        remaining -= n;
    }

    fclose(file_fp);

    printf("[SUCCESS] User: %s\n", username);
    printf("[SUCCESS] File: %s\n", filename);
    printf("[SUCCESS] Size: %llu bytes\n", file_size);
    printf("[SUCCESS] Stored: %s\n", user_file_path);

    return 0;
}

/* --------------------------------------------------
   Handle one connected client
   -------------------------------------------------- */
void handle_client(int client_fd)
{
    unsigned int username_len;
    unsigned int filename_len;
    unsigned long long file_size;

    char username[256];
    char filename[256];

    /* -----------------------------------------------
       Receive username length
       ----------------------------------------------- */
    if (recv_all(client_fd,
                 &username_len,
                 sizeof(username_len)) < 0)
    {
        printf("[ERROR] Failed to receive username length\n");
        return;
    }

    if (username_len == 0 || username_len >= sizeof(username))
    {
        printf("[ERROR] Invalid username length\n");
        return;
    }

    /* Receive username */
    if (recv_all(client_fd,
                 username,
                 username_len) < 0)
    {
        printf("[ERROR] Failed to receive username\n");
        return;
    }

    username[username_len] = '\0';

    /* -----------------------------------------------
       Receive filename length
       ----------------------------------------------- */
    if (recv_all(client_fd,
                 &filename_len,
                 sizeof(filename_len)) < 0)
    {
        printf("[ERROR] Failed to receive filename length\n");
        return;
    }

    if (filename_len == 0 || filename_len >= sizeof(filename))
    {
        printf("[ERROR] Invalid filename length\n");
        return;
    }

    /* Receive filename */
    if (recv_all(client_fd,
                 filename,
                 filename_len) < 0)
    {
        printf("[ERROR] Failed to receive filename\n");
        return;
    }

    filename[filename_len] = '\0';

    /* -----------------------------------------------
       Receive file size
       ----------------------------------------------- */
    if (recv_all(client_fd,
                 &file_size,
                 sizeof(file_size)) < 0)
    {
        printf("[ERROR] Failed to receive file size\n");
        return;
    }

    printf("\n[CLIENT REQUEST]\n");
    printf("Username : %s\n", username);
    printf("Filename : %s\n", filename);
    printf("Size     : %llu bytes\n",
           file_size);

    /* -----------------------------------------------
       Receive and store file
       ----------------------------------------------- */
    if (process_user_file_upload(
            client_fd,
            username,
            filename,
            file_size) < 0)
    {
        printf("[ERROR] File upload failed\n");
        return;
    }

    /* Tell client upload succeeded */
    const char *response = "UPLOAD_SUCCESS";

    send(client_fd,
         response,
         strlen(response),
         0);
}

/* --------------------------------------------------
   Main server
   -------------------------------------------------- */
int main(void)
{
    int server_fd;
    int client_fd;

    struct sockaddr_in server_addr;
    struct sockaddr_in client_addr;

    socklen_t client_len =
        sizeof(client_addr);

    /* Initialize storage */
    init_file_system();

    /* ------------------------------------------------
       Create socket
       ------------------------------------------------ */
    server_fd = socket(AF_INET,
                       SOCK_STREAM,
                       0);

    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    /* Allow immediate reuse of port */
    int opt = 1;

    setsockopt(server_fd,
               SOL_SOCKET,
               SO_REUSEADDR,
               &opt,
               sizeof(opt));

    /* ------------------------------------------------
       Configure server address
       ------------------------------------------------ */
    memset(&server_addr, 0,
           sizeof(server_addr));

    server_addr.sin_family = AF_INET;

    server_addr.sin_addr.s_addr =
        INADDR_ANY;

    server_addr.sin_port =
        htons(PORT);

    /* ------------------------------------------------
       Bind socket to port
       ------------------------------------------------ */
    if (bind(server_fd,
             (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0)
    {
        perror("bind");
        close(server_fd);
        return 1;
    }

    /* ------------------------------------------------
       Listen
       ------------------------------------------------ */
    if (listen(server_fd, 10) < 0)
    {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("=====================================\n");
    printf(" High-Speed Transfer Server\n");
    printf(" Listening on port %d\n", PORT);
    printf("=====================================\n");

    /* ------------------------------------------------
       Accept clients continuously
       ------------------------------------------------ */
    while (1)
    {
        printf("\nWaiting for client...\n");

        client_fd = accept(
            server_fd,
            (struct sockaddr *)&client_addr,
            &client_len);

        if (client_fd < 0)
        {
            perror("accept");
            continue;
        }

        printf("[CONNECTED] Client: %s\n",
               inet_ntoa(client_addr.sin_addr));

        /* Process client */
        handle_client(client_fd);

        close(client_fd);

        printf("[DISCONNECTED]\n");
    }

    close(server_fd);

    return 0;
}
