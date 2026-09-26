#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>

#define USERS_DIR "storage/users"
#define BUFFER_SIZE 8192
#define SERVER_IP "127.0.0.1"

int recv_all(int sockfd, void *buffer, size_t size)
{
    char *ptr = (char *)buffer;
    size_t received = 0;
    while (received < size)
    {
        ssize_t n = recv(sockfd, ptr + received, size - received, 0);
        if (n == 0)
        {
            return -1;   // Connection closed
        }
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            return -1;
        }
        received += n;
    }
    return 0;
}

int user_exists(const char *username)
{
    char path[512];
    snprintf(path, sizeof(path), "%s/%s", USERS_DIR, username);
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return 1;
    return 0;
}

int create_destination_file(const char *username, const char *filename, char *output_path, size_t output_size)
{
    snprintf(output_path, output_size, "%s/%s/%s", USERS_DIR, username, filename);
    FILE *fp = fopen(output_path, "wb");
    if (!fp)
    {
        perror("fopen destination");
        return -1;
    }
    fclose(fp);
    return 0;
}

void receive_from_port(int port, const char *file_path)
{
    int sockfd;
    struct sockaddr_in server_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr);

    printf("[CHILD] Connecting to %s:%d...\n", SERVER_IP, port);
    if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        perror("connect");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    printf("[CHILD] Connected to port %d\n", port);

    FILE *fp = fopen(file_path, "wb");
    if (!fp)
    {
        perror("fopen output file");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    unsigned long long total_received = 0;

    while (1)
    {
        ssize_t n = recv(sockfd, buffer, sizeof(buffer), 0);

        if (n == 0)
        {
            break;
        }
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            perror("recv");
            fclose(fp);
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        size_t written = fwrite(buffer, 1, n, fp);
        if (written != (size_t)n)
        {
            perror("fwrite");
            fclose(fp);
            close(sockfd);
            exit(EXIT_FAILURE);
        }

        total_received += n;
        printf("\r[CHILD] Received: %llu bytes", total_received);
        fflush(stdout);
    }

    printf("\n[CHILD] Transfer completed.\n");
    printf("[CHILD] Total received: %llu bytes\n", total_received);

    fclose(fp);
    close(sockfd);
    exit(EXIT_SUCCESS);
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage: %s <username> <port> <filename>\n", argv[0]);
        printf("\nExample:\n");
        printf("  %s aravind 9000 video.mp4\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *username = argv[1];
    int port = atoi(argv[2]);
    const char *filename = argv[3];

    if (port <= 0 || port > 65535)
    {
        printf("[ERROR] Invalid port number.\n");
        return EXIT_FAILURE;
    }

    if (!user_exists(username))
    {
        printf("[ERROR] User '%s' does not exist.\n", username);
        return EXIT_FAILURE;
    }
    printf("[OK] User '%s' exists.\n", username);

    char file_path[512];
    if (create_destination_file(username, filename, file_path, sizeof(file_path)) < 0)
    {
        return EXIT_FAILURE;
    }
    printf("[OK] Created file: %s\n", file_path);

    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return EXIT_FAILURE;
    }

    if (pid == 0)
    {
        printf("[CHILD] PID = %d\n", getpid());
        receive_from_port(port, file_path);
    }

    printf("[PARENT] PID = %d\n", getpid());
    printf("[PARENT] Created transfer process PID = %d\n", pid);
    printf("[PARENT] Waiting for transfer process...\n");
    waitpid(pid, NULL, 0);
    printf("[PARENT] Transfer process finished.\n");

    return EXIT_SUCCESS;
}

/**
File Receiver and Port-Based File Storage1. PurposeThis program receives three inputs:UsernamePortFilenameExample:Bash./file_receiver aravind 9000 video.mp4
The program performs the following operations:Checks whether the user directory exists.Creates the requested destination file inside the user's directory.Creates a separate child process using fork().The child process connects to the specified TCP port.The child receives data from the port.The received data is written into the destination file.The parent process waits until the child finishes.2. Directory StructureThe program expects the following structure:Plaintextstorage/
└── users/
    ├── aravind/
    └── rahul/
The user directory must already exist. For example:Plaintextstorage/users/aravind/
If the directory does not exist, the program reports an error and stops. File listings are determined directly by inspecting the user's directory contents on the filesystem (e.g., using opendir()/readdir()), without relying on secondary tracking files.3. Input FormatThe program is executed using:Bash./file_receiver <username> <port> <filename>
Example:Bash./file_receiver aravind 9000 video.mp4
The arguments map to:argv[0] = ./file_receiverargv[1] = username (aravind)argv[2] = port (9000)argv[3] = filename (video.mp4)4. Overall ArchitecturePlaintext                    USER INPUT
                        |
            username / port / filename
                        |
                        v
               +-----------------+
               |   Main Process  |
               +--------+--------+
                        |
                        v
               Check User Directory
                        |
                        v
                   Create File
                        |
                        v
                      fork()
                   +----+----+
                   |         |
                   v         v
                Parent     Child
                   |         |
                   |         v
                   |      socket()
                   |         |
                   |         v
                   |      connect()
                   |         |
                   |         v
                   |       recv()
                   |         |
                   |         v
                   |       buffer
                   |         |
                   |         v
                   |      fwrite()
                   |         |
                   |         v
                   |  Destination File
                   |
                   v
               waitpid()
                   |
                   v
                 DONE
5. Main ComponentsThe program is divided into the following logical components:Input HandlingUser ValidationFile CreationProcess Creation (fork())Network Data Reception & Disk Writing6. Input HandlingThe program extracts command-line arguments using argv[]:Cconst char *username = argv[1];
int port = atoi(argv[2]);
const char *filename = argv[3];
Validation ensures that the provided port falls within the valid range (1–65535).7. User Directory ValidationThe user_exists() function checks whether the user's directory exists by constructing the path:Plaintextstorage/users/<username>
The stat() system call and S_ISDIR() macro verify that the target path exists and is indeed a directory.Plaintextstorage/users/aravind
          |
          v
        stat()
          |
     +----+----+
     |         |
   Exists  Does not exist
     |         |
     v         v
 Continue    Error
8. Destination File CreationAfter verifying that the user directory exists, the program creates the destination file:Plaintextstorage/users/<username>/<filename>
The file is initialized in binary write mode:Cfopen(path, "wb");
Binary mode (wb) ensures raw byte streams (images, videos, archives, executables) are written accurately without platform-specific text translations.9. Process Creation (fork())The main process spawns a dedicated worker process using fork():Plaintext              file_receiver
                    |
                  fork()
                +---+---+
                |       |
                v       v
             Parent   Child
Parent Process: Acts as the supervisor, executing waitpid() to monitor child execution until completion.Child Process: Handles network I/O and file writing, then exits upon completion.10. TCP Socket & ConnectionThe child process creates an IPv4 TCP socket and connects to the source address:Csocket(AF_INET, SOCK_STREAM, 0);
It connects to 127.0.0.1:<port> using connect(). A data provider must be actively listening on that port.PlaintextData Source (:9000) <--- TCP Connection ---> Child Receiver Process
11. Receiving and Writing DataData is received in chunks into an 8 KB buffer:Cchar buffer[8192];
The transfer loop operates as follows:PlaintextNetwork -> recv() -> Buffer -> fwrite() -> Destination File
recv() fetches incoming bytes into buffer.fwrite() writes exact byte counts (n) to the open file pointer.n == 0 signals normal peer disconnection (EOF), breaking the loop.12. Complete Execution LifecyclePlaintext 1. Parse username, port, filename.
 2. Verify existence of storage/users/<username>/.
 3. Initialize target file storage/users/<username>/<filename>.
 4. Call fork().
 5. Child connects to 127.0.0.1:<port>.
 6. Child streams data into the target file until EOF.
 7. Child closes socket/file and exits with status 0.
 8. Parent process catches child exit via waitpid() and finishes.
13. System Calls & Standard Functions UsedFunctionPurposestat()Checks if the user directory existsfopen() / fwrite() / fclose()Handles file stream lifecycle and binary disk writesfork()Spawns a child process for isolated network executionwaitpid()Blocks parent process until child process terminatessocket()Allocates TCP network socketconnect()Establishes connection to remote/local portrecv()Receives raw socket payload
*/
