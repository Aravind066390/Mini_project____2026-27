#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>
#include <sys/wait.h>
#include <dirent.h>

#define USERS_DIR "storage/users"
#define BUFFER_SIZE 8192

/* ---------------------------------------------------------
   Check whether user directory exists
   --------------------------------------------------------- */
int user_exists(const char *username)
{
    char path[512];

    snprintf(path, sizeof(path), "%s/%s", USERS_DIR, username);

    struct stat st;

    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return 1;

    return 0;
}

/* ---------------------------------------------------------
   Send a single complete file through TCP
   --------------------------------------------------------- */
int send_file(int sockfd, const char *file_path)
{
    FILE *fp = fopen(file_path, "rb");

    if (!fp)
    {
        perror("[ERROR] fopen source file");
        return -1;
    }

    char buffer[BUFFER_SIZE];
    unsigned long long total_sent = 0;

    while (1)
    {
        size_t n = fread(buffer, 1, sizeof(buffer), fp);

        if (n == 0)
        {
            if (ferror(fp))
            {
                perror("[ERROR] fread");
                fclose(fp);
                return -1;
            }
            /* End of file */
            break;
        }

        size_t sent = 0;

        while (sent < n)
        {
            ssize_t s = send(sockfd, buffer + sent, n - sent, 0);

            if (s < 0)
            {
                if (errno == EINTR)
                    continue;

                perror("[ERROR] send");
                fclose(fp);
                return -1;
            }

            if (s == 0)
            {
                printf("\n[ERROR] Connection closed while sending.\n");
                fclose(fp);
                return -1;
            }

            sent += s;
            total_sent += s;

            printf("\r[SENDER] Sent: %llu bytes", total_sent);
            fflush(stdout);
        }
    }

    printf("\n[SENDER] File transfer completed (%llu bytes).\n", total_sent);
    fclose(fp);
    return 0;
}

/* ---------------------------------------------------------
   Connect to destination
   --------------------------------------------------------- */
int connect_to_destination(const char *ip, int port)
{
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);

    if (sockfd < 0)
    {
        perror("[ERROR] socket");
        return -1;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &server_addr.sin_addr) <= 0)
    {
        printf("[ERROR] Invalid IP address: %s\n", ip);
        close(sockfd);
        return -1;
    }

    printf("[SENDER] Connecting to %s:%d...\n", ip, port);

    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("[ERROR] connect");
        close(sockfd);
        return -1;
    }

    printf("[SENDER] Connected to %s:%d\n", ip, port);
    return sockfd;
}

/* ---------------------------------------------------------
   Iterate directory and send all regular files
   --------------------------------------------------------- */
int send_user_directory_files(int sockfd, const char *username)
{
    char dir_path[512];
    snprintf(dir_path, sizeof(dir_path), "%s/%s", USERS_DIR, username);

    DIR *dir = opendir(dir_path);
    if (!dir)
    {
        perror("[ERROR] opendir");
        return -1;
    }

    struct dirent *entry;
    int files_sent = 0;

    while ((entry = readdir(dir)) != NULL)
    {
        /* Ignore hidden files, '.' and '..' */
        if (entry->d_name[0] == '.')
            continue;

        char file_path[1024];
        snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(file_path, &st) != 0)
        {
            perror("[ERROR] stat");
            continue;
        }

        /* Ensure it is a regular file */
        if (S_ISREG(st.st_mode))
        {
            printf("\n----------------------------------------\n");
            printf("[OK] Processing File: %s\n", entry->d_name);
            printf("[OK] File path: %s\n", file_path);
            printf("[OK] File size: %lld bytes\n", (long long)st.st_size);

            if (send_file(sockfd, file_path) < 0)
            {
                printf("[ERROR] Failed to send file: %s\n", entry->d_name);
                closedir(dir);
                return -1;
            }

            files_sent++;
        }
    }

    closedir(dir);

    if (files_sent == 0)
    {
        printf("\n[INFO] No regular files found in directory '%s'.\n", dir_path);
    }
    else
    {
        printf("\n========================================\n");
        printf("[OK] Total files sent successfully: %d\n", files_sent);
    }

    return 0;
}

/* ---------------------------------------------------------
   Main
   --------------------------------------------------------- */
int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage: %s <username> <IP> <port>\n", argv[0]);
        printf("\nExample:\n");
        printf("  %s aravind 192.168.1.20 9000\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *username = argv[1];
    const char *ip = argv[2];
    int port = atoi(argv[3]);

    /* Validate port */
    if (port <= 0 || port > 65535)
    {
        printf("[ERROR] Invalid port number.\n");
        return EXIT_FAILURE;
    }

    /* Check user */
    if (!user_exists(username))
    {
        printf("[ERROR] User '%s' does not exist.\n", username);
        return EXIT_FAILURE;
    }

    printf("[OK] User '%s' exists.\n", username);

    /* Connect to destination */
    int sockfd = connect_to_destination(ip, port);
    if (sockfd < 0)
    {
        return EXIT_FAILURE;
    }

    /* Process directory and send files */
    int result = send_user_directory_files(sockfd, username);

    /* Signal EOF to the receiver */
    shutdown(sockfd, SHUT_WR);
    close(sockfd);

    if (result < 0)
    {
        printf("[ERROR] Directory transfer failed.\n");
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

/**

Directory-Based File Sender Documentation1. PurposeThis program reads files directly from a user's storage directory (storage/users/<username>/) using standard filesystem directory scanning (opendir()/readdir()) and transfers all valid regular files over a TCP connection to a destination IP and port.2. Updated Program Execution StepsPlaintextStep 1:
Check user directory existence (storage/users/aravind/)

Step 2:
Connect via TCP socket to 192.168.1.20:9000

Step 3:
Open directory using opendir()

Step 4:
Loop through directory entries using readdir()

Step 5:
Filter out hidden items (e.g., '.', '..') and verify regular file status via stat()

Step 6:
For each regular file:
    - Open file in binary mode ("rb")
    - Read payload via fread()
    - Send payload via send()
    - Close file upon EOF

Step 7:
Close directory handle (closedir())

Step 8:
Issue shutdown(sockfd, SHUT_WR) to inform remote peer transfer completion

Step 9:
Close socket and exit
3. System Calls & Functions UsedFunctionPurposeopendir()Opens directory stream for readingreaddir()Reads successive directory entries (struct dirent)closedir()Closes directory stream handlestat() / S_ISREG()Retrieves metadata and checks if an entry is a regular filesocket() / connect()Allocates TCP socket and connects to serverfread() / send()Streams file data in 8 KB chunksshutdown()Signals TCP EOF (FIN) to receiver


*/
