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

#define USERS_DIR "storage/users"
#define BUFFER_SIZE 8192


/* ---------------------------------------------------------
   Check whether user directory exists
   --------------------------------------------------------- */
int user_exists(const char *username)
{
    char path[512];

    snprintf(path, sizeof(path),
             "%s/%s",
             USERS_DIR,
             username);

    struct stat st;

    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return 1;

    return 0;
}


/* ---------------------------------------------------------
   Read the first filename from data.txt
   --------------------------------------------------------- */
int get_first_filename(const char *username,
                       char *filename,
                       size_t filename_size)
{
    char path[512];

    snprintf(path, sizeof(path),
             "%s/%s/data.txt",
             USERS_DIR,
             username);

    FILE *fp = fopen(path, "r");

    if (!fp)
    {
        perror("fopen data.txt");
        return -1;
    }

    /*
       Read the first line.
    */
    if (fgets(filename, filename_size, fp) == NULL)
    {
        fclose(fp);

        /*
           Empty data.txt
        */
        return 1;
    }

    fclose(fp);

    /*
       Remove newline.
    */
    filename[strcspn(filename, "\r\n")] = '\0';

    if (filename[0] == '\0')
        return 1;

    return 0;
}


/* ---------------------------------------------------------
   Remove the first entry from data.txt

   Example:

   Before:

   video.mp4
   image.jpg
   notes.txt

   After:

   image.jpg
   notes.txt
   --------------------------------------------------------- */
int remove_first_entry(const char *username)
{
    char data_path[512];
    char temp_path[512];

    snprintf(data_path, sizeof(data_path),
             "%s/%s/data.txt",
             USERS_DIR,
             username);

    snprintf(temp_path, sizeof(temp_path),
             "%s/%s/data.tmp",
             USERS_DIR,
             username);

    FILE *src = fopen(data_path, "r");

    if (!src)
    {
        perror("fopen data.txt");
        return -1;
    }

    FILE *tmp = fopen(temp_path, "w");

    if (!tmp)
    {
        perror("fopen data.tmp");
        fclose(src);
        return -1;
    }

    char buffer[1024];

    /*
       Skip the first line.
    */
    if (fgets(buffer, sizeof(buffer), src) == NULL)
    {
        fclose(src);
        fclose(tmp);

        remove(temp_path);

        return -1;
    }

    /*
       Copy all remaining entries.
    */
    while (fgets(buffer, sizeof(buffer), src) != NULL)
    {
        fputs(buffer, tmp);
    }

    fclose(src);
    fclose(tmp);

    /*
       Replace original data.txt.
    */
    if (rename(temp_path, data_path) != 0)
    {
        perror("rename");

        remove(temp_path);

        return -1;
    }

    return 0;
}


/* ---------------------------------------------------------
   Send the complete file through TCP
   --------------------------------------------------------- */
int send_file(int sockfd, const char *file_path)
{
    FILE *fp = fopen(file_path, "rb");

    if (!fp)
    {
        perror("fopen source file");
        return -1;
    }

    char buffer[BUFFER_SIZE];

    unsigned long long total_sent = 0;

    while (1)
    {
        size_t n = fread(buffer, 1,
                         sizeof(buffer),
                         fp);

        if (n == 0)
        {
            if (ferror(fp))
            {
                perror("fread");
                fclose(fp);
                return -1;
            }

            /*
               End of file.
            */
            break;
        }

        size_t sent = 0;

        while (sent < n)
        {
            ssize_t s = send(sockfd,
                             buffer + sent,
                             n - sent,
                             0);

            if (s < 0)
            {
                if (errno == EINTR)
                    continue;

                perror("send");

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

            printf("\r[SENDER] Sent: %llu bytes",
                   total_sent);

            fflush(stdout);
        }
    }

    printf("\n[SENDER] Transfer completed.\n");
    printf("[SENDER] Total sent: %llu bytes\n",
           total_sent);

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

    sockfd = socket(AF_INET,
                    SOCK_STREAM,
                    0);

    if (sockfd < 0)
    {
        perror("socket");
        return -1;
    }

    memset(&server_addr,
           0,
           sizeof(server_addr));

    server_addr.sin_family = AF_INET;

    server_addr.sin_port = htons(port);

    if (inet_pton(AF_INET,
                  ip,
                  &server_addr.sin_addr) <= 0)
    {
        printf("[ERROR] Invalid IP address: %s\n",
               ip);

        close(sockfd);

        return -1;
    }

    printf("[SENDER] Connecting to %s:%d...\n",
           ip,
           port);

    if (connect(sockfd,
                (struct sockaddr *)&server_addr,
                sizeof(server_addr)) < 0)
    {
        perror("connect");

        close(sockfd);

        return -1;
    }

    printf("[SENDER] Connected to %s:%d\n",
           ip,
           port);

    return sockfd;
}


/* ---------------------------------------------------------
   Main
   --------------------------------------------------------- */
int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage: %s <username> <IP> <port>\n",
               argv[0]);

        printf("\nExample:\n");

        printf("  %s aravind 192.168.1.20 9000\n",
               argv[0]);

        return EXIT_FAILURE;
    }

    const char *username = argv[1];

    const char *ip = argv[2];

    int port = atoi(argv[3]);


    /* -----------------------------------------------------
       Validate port
       ----------------------------------------------------- */
    if (port <= 0 || port > 65535)
    {
        printf("[ERROR] Invalid port number.\n");

        return EXIT_FAILURE;
    }


    /* -----------------------------------------------------
       Check user
       ----------------------------------------------------- */
    if (!user_exists(username))
    {
        printf("[ERROR] User '%s' does not exist.\n",
               username);

        return EXIT_FAILURE;
    }

    printf("[OK] User '%s' exists.\n",
           username);


    /* -----------------------------------------------------
       Get first pending file
       ----------------------------------------------------- */

    char filename[512];

    int result =
        get_first_filename(username,
                           filename,
                           sizeof(filename));

    if (result < 0)
    {
        return EXIT_FAILURE;
    }

    if (result == 1)
    {
        printf("[INFO] data.txt is empty.\n");

        return EXIT_SUCCESS;
    }

    printf("[OK] Next file: %s\n",
           filename);


    /* -----------------------------------------------------
       Construct actual file path
       ----------------------------------------------------- */

    char file_path[1024];

    snprintf(file_path,
             sizeof(file_path),
             "%s/%s/%s",
             USERS_DIR,
             username,
             filename);

    printf("[OK] Source file: %s\n",
           file_path);


    /* -----------------------------------------------------
       Check source file
       ----------------------------------------------------- */

    struct stat st;

    if (stat(file_path, &st) != 0)
    {
        perror("[ERROR] Source file");

        return EXIT_FAILURE;
    }

    if (!S_ISREG(st.st_mode))
    {
        printf("[ERROR] Source is not a regular file.\n");

        return EXIT_FAILURE;
    }


    printf("[OK] File size: %lld bytes\n",
           (long long)st.st_size);


    /* -----------------------------------------------------
       Connect to destination
       ----------------------------------------------------- */

    int sockfd =
        connect_to_destination(ip, port);

    if (sockfd < 0)
    {
        return EXIT_FAILURE;
    }


    /* -----------------------------------------------------
       Send file
       ----------------------------------------------------- */

    int send_result =
        send_file(sockfd, file_path);


    /*
       Closing the socket tells the receiver:

       "No more data is coming."
    */
    shutdown(sockfd, SHUT_WR);

    close(sockfd);


    /* -----------------------------------------------------
       IMPORTANT:

       Remove entry ONLY if transfer succeeded.
       ----------------------------------------------------- */

    if (send_result == 0)
    {
        printf("[OK] File sent successfully.\n");

        printf("[SENDER] Removing '%s' from data.txt...\n",
               filename);

        if (remove_first_entry(username) != 0)
        {
            printf("[ERROR] File was sent, "
                   "but data.txt could not be updated.\n");

            return EXIT_FAILURE;
        }

        printf("[OK] Removed '%s' from data.txt.\n",
               filename);
    }
    else
    {
        printf("[ERROR] Transfer failed.\n");

        printf("[INFO] '%s' remains in data.txt.\n",
               filename);

        return EXIT_FAILURE;
    }


    return EXIT_SUCCESS;
}
/**
Step 1:
Check user aravind

Step 2:
Open data.txt

Step 3:
Read first entry

        video.mp4

Step 4:
Construct:

storage/users/aravind/video.mp4

Step 5:
Check file

Step 6:
Create TCP socket

Step 7:
Connect to:

192.168.1.20:9000

Step 8:
Open video.mp4

Step 9:
Read file using fread()

Step 10:
Send data using send()

Step 11:
Reach EOF

Step 12:
shutdown(SHUT_WR)

Step 13:
Close socket

Step 14:
Remove video.mp4 from data.txt
  */
