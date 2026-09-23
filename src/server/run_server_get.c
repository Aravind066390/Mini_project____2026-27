#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
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
        ssize_t n = recv(sockfd,ptr + received,size - received,0);
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
    snprintf(path,sizeof(path),"%s/%s",USERS_DIR,username);
    struct stat st;
    if (stat(path, &st) == 0 && S_ISDIR(st.st_mode))
        return 1;
    return 0;
}
int add_filename_to_data(const char *username,const char *filename)
{
    char path[512];
    snprintf(path,sizeof(path),"%s/%s/data.txt",USERS_DIR,username);
    FILE *fp = fopen(path, "a");
    if (!fp)
    {
        perror("fopen data.txt");
        return -1;
    }
    fprintf(fp, "%s\n", filename);
    fclose(fp);
    return 0;
}
int create_destination_file(const char *username,const char *filename,char *output_path,size_t output_size)
{
    snprintf(output_path,output_size,"%s/%s/%s",USERS_DIR,username,filename);
    FILE *fp = fopen(output_path, "wb");
    if (!fp)
    {
        perror("fopen destination");
        return -1;
    }
    fclose(fp);
    return 0;
}
void receive_from_port(int port,const char *file_path){
    int sockfd;
    struct sockaddr_in server_addr;
    sockfd = socket(AF_INET,SOCK_STREAM,0);
    if (sockfd < 0)
    {
        perror("socket");
        exit(EXIT_FAILURE);
    }
    memset(&server_addr, 0,
           sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET,SERVER_IP,&server_addr.sin_addr);
    printf("[CHILD] Connecting to %s:%d...\n",SERVER_IP,port);
    if (connect(sockfd,(struct sockaddr *)&server_addr,sizeof(server_addr)) < 0)
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
        ssize_t n = recv(sockfd,buffer,sizeof(buffer),0);

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
        size_t written =fwrite(buffer,1,n,fp);
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
    printf("[CHILD] Total received: %llu bytes\n",total_received);
    fclose(fp);
    close(sockfd);
    exit(EXIT_SUCCESS);
}
int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        printf("Usage: %s <username> <port> <filename>\n",
               argv[0]);

        printf("\nExample:\n");
        printf("  %s aravind 9000 video.mp4\n",
               argv[0]);

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
        printf("[ERROR] User '%s' does not exist.\n",
               username);

        return EXIT_FAILURE;
    }
    printf("[OK] User '%s' exists.\n",
           username);
    char file_path[512];
    if (create_destination_file(username,filename,file_path,sizeof(file_path)) < 0)
    {
        return EXIT_FAILURE;
    }
    printf("[OK] Created file: %s\n",file_path);
    if (add_filename_to_data(username,filename) < 0)
    {
        return EXIT_FAILURE;
    }
    printf("[OK] Updated data.txt\n");
    pid_t pid = fork();
    if (pid < 0)
    {
        perror("fork");
        return EXIT_FAILURE;
    }
    if (pid == 0)
    {
        printf("[CHILD] PID = %d\n",
               getpid());
        receive_from_port(port,
                          file_path);
    }
    printf("[PARENT] PID = %d\n",
           getpid());
    printf("[PARENT] Created transfer process PID = %d\n",
           pid);
    printf("[PARENT] Waiting for transfer process...\n");
    waitpid(pid, NULL, 0);
    printf("[PARENT] Transfer process finished.\n");
    return EXIT_SUCCESS;
}
# File Receiver and Port-Based File Storage

## 1. Purpose

This program receives three inputs:

- Username
- Port
- Filename

Example:

    ./file_receiver aravind 9000 video.mp4

The program performs the following operations:

1. Checks whether the user directory exists.
2. Creates the requested file inside the user's directory.
3. Adds the filename to `data.txt`.
4. Creates a separate child process using `fork()`.
5. The child process connects to the specified TCP port.
6. The child receives data from the port.
7. The received data is written into the newly created file.
8. The parent process waits until the child finishes.

---

# 2. Directory Structure

The program expects the following structure:

    storage/
    └── users/
        ├── aravind/
        │   └── data.txt
        │
        └── rahul/
            └── data.txt

The user directory must already exist.

For example:

    storage/users/aravind/

If the directory does not exist, the program reports an error and stops.

---

# 3. Input Format

The program is executed using:

    ./file_receiver <username> <port> <filename>

Example:

    ./file_receiver aravind 9000 video.mp4

The arguments are:

    username = aravind
    port     = 9000
    filename = video.mp4

---

# 4. Overall Architecture

The program follows this architecture:

                    USER INPUT
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
                 Update data.txt
                        |
                        v
                      fork()
                   +----+----+
                   |         |
                   v         v
                Parent     Child
                   |         |
                   |         v
                   |       socket()
                   |         |
                   |         v
                   |      connect()
                   |         |
                   |         v
                   |        recv()
                   |         |
                   |         v
                   |       buffer
                   |         |
                   |         v
                   |      fwrite()
                   |         |
                   |         v
                   |    Destination File
                   |
                   v
                waitpid()
                   |
                   v
                  DONE

---

# 5. Main Components

The program can be divided into the following logical components:

1. Input Handling
2. User Validation
3. File Creation
4. Metadata Update
5. Process Creation
6. Network Data Reception

---

# 6. Input Handling

The program gets the command-line arguments using `argv[]`.

Example:

    ./file_receiver aravind 9000 video.mp4

The values are stored as:

    argv[0] = ./file_receiver
    argv[1] = aravind
    argv[2] = 9000
    argv[3] = video.mp4

The program uses:

    const char *username = argv[1];
    int port = atoi(argv[2]);
    const char *filename = argv[3];

Therefore:

    username -> aravind
    port     -> 9000
    filename -> video.mp4

---

# 7. User Directory Validation

The function:

    user_exists()

checks whether the user's directory exists.

The program constructs:

    storage/users/<username>

For example:

    storage/users/aravind

The `stat()` system call is used to check the filesystem object.

The program also checks:

    S_ISDIR()

This confirms that the path is actually a directory.

The logic is:

    storage/users/aravind
             |
             v
           stat()
             |
        +----+----+
        |         |
      Exists    Does not exist
        |         |
        v         v
     Continue    Error

---

# 8. Destination File Creation

After confirming that the user exists, the program creates the destination file.

The path is:

    storage/users/<username>/<filename>

Example:

    storage/users/aravind/video.mp4

The file is opened using:

    fopen(path, "wb");

`wb` means:

    w = write
    b = binary

Binary mode is important because the program may receive:

- Video
- Image
- PDF
- ZIP
- Binary data
- Text files

The program should treat the incoming information as raw bytes.

---

# 9. Updating data.txt

The filename is added to:

    storage/users/<username>/data.txt

For example:

    storage/users/aravind/data.txt

The file is opened using:

    fopen(path, "a");

`a` means append.

Suppose `data.txt` initially contains:

    notes.txt
    image.jpg

After uploading `video.mp4`, it becomes:

    notes.txt
    image.jpg
    video.mp4

Therefore, `data.txt` acts as a simple file list or metadata record.

---

# 10. Creating a Separate Process

The program uses:

    fork();

`fork()` creates a new process.

Before `fork()`:

    file_receiver
         |
         +-- Main Process

After `fork()`:

                 file_receiver
                       |
                     fork()
                   +---+---+
                   |       |
                   v       v
                Parent   Child

The parent and child are now separate processes.

---

# 11. Parent Process

The parent process is responsible for managing the operation.

The parent does not receive the actual file data.

It executes:

    waitpid(pid, NULL, 0);

This means:

    Wait until the child process finishes.

The parent therefore acts as the process manager.

Its basic responsibilities are:

- Start the operation
- Create the child
- Wait for the child
- Detect completion

---

# 12. Child Process

The child process performs the actual network transfer.

The child calls:

    receive_from_port(port, file_path);

For example:

    port = 9000

    file_path =
    storage/users/aravind/video.mp4

The child is responsible for:

    Network
       |
       v
    Receive Data
       |
       v
    Write Data
       |
       v
    Destination File

---

# 13. TCP Socket Creation

The child creates a TCP socket using:

    socket(AF_INET, SOCK_STREAM, 0);

The parameters mean:

    AF_INET
        IPv4

    SOCK_STREAM
        TCP

Therefore:

    socket()
       |
       v
    IPv4 TCP socket

---

# 14. Connecting to the Data Source

The current program uses:

    127.0.0.1

This represents the local computer.

If the port is:

    9000

the child attempts to connect to:

    127.0.0.1:9000

using:

    connect();

The architecture is:

    Data Producer
         |
         | listen()
         |
         | TCP port 9000
         |
         v
       Network
         |
         v
    Child Receiver
         |
         | connect()
         |
         v
    127.0.0.1:9000

Another process must therefore be listening on the specified port.

---

# 15. Receiving Data

The child creates a temporary buffer:

    char buffer[8192];

This buffer can hold up to 8192 bytes at a time.

The child receives data using:

    recv();

The basic flow is:

    Data Producer
          |
          | TCP data
          v
        recv()
          |
          v
        buffer
          |
          v
       fwrite()
          |
          v
      Destination File

The complete file does not need to be stored in RAM at once.

For example, a 10 MB file can be received in multiple chunks.

---

# 16. Writing Data to the File

After receiving data into the buffer, the program uses:

    fwrite(buffer, 1, n, fp);

to write the received bytes into the destination file.

The flow is:

    Network
       |
       v
     recv()
       |
       v
    Buffer
       |
       v
    fwrite()
       |
       v
    video.mp4

This allows the program to handle binary files.

---

# 17. Detecting the End of Transfer

The current implementation assumes that the sender closes the TCP connection after sending the complete file.

When the sender closes the connection:

    recv()

returns:

    0

Therefore:

    if (n == 0)
        break;

means:

    The sender has closed the connection.
    The transfer is complete.

The sequence is:

    Sender
       |
       +-- send data
       |
       +-- send data
       |
       +-- send data
       |
       +-- close()
              |
              v
          Receiver
              |
            recv()
              |
              v
              0
              |
              v
       Transfer Complete

---

# 18. Closing the Connection

After the transfer is finished, the child closes:

    fclose(fp);

and:

    close(sockfd);

Then the child terminates:

    exit(EXIT_SUCCESS);

The resources are therefore released.

---

# 19. Parent-Child Interaction

The complete process relationship is:

                     Parent
                       |
                     fork()
                       |
              +--------+--------+
              |                 |
              v                 v
           Parent             Child
              |                 |
              |              socket()
              |                 |
              |              connect()
              |                 |
              |               recv()
              |                 |
              |              fwrite()
              |                 |
              |              File
              |                 |
              |              exit()
              |                 |
              +---- waitpid() --+
                       |
                       v
                     DONE

---

# 20. Complete Data Flow

The complete data flow is:

    Data Producer
         |
         | TCP
         v
    Linux TCP Stack
         |
         v
       recv()
         |
         v
    User-space Buffer
         |
         v
      fwrite()
         |
         v
     File System
         |
         v
    video.mp4

The resulting file is:

    storage/users/aravind/video.mp4

---

# 21. Example Execution

Suppose the following directory exists:

    storage/users/aravind/

Run:

    ./file_receiver aravind 9000 video.mp4

The program performs:

    Step 1:
    Check storage/users/aravind/

    Step 2:
    Create storage/users/aravind/video.mp4

    Step 3:
    Add video.mp4 to data.txt

    Step 4:
    fork()

    Step 5:
    Child connects to 127.0.0.1:9000

    Step 6:
    Child receives data

    Step 7:
    Child writes data into video.mp4

    Step 8:
    Sender closes connection

    Step 9:
    Child exits

    Step 10:
    Parent detects completion

---

# 22. Important System Calls and Functions

The most important functions used in this program are:

    socket()
        Creates a socket.

    connect()
        Connects to another process through TCP.

    recv()
        Receives data from a socket.

    close()
        Closes a socket.

    fork()
        Creates a child process.

    waitpid()
        Waits for a child process.

    stat()
        Gets filesystem information.

    fopen()
        Opens or creates a file.

    fwrite()
        Writes binary data to a file.

    fclose()
        Closes a file.

---

# 23. Current Data Path

The current implementation is a conventional TCP-based baseline:

    Network
       |
       v
      TCP
       |
       v
     recv()
       |
       v
    User Buffer
       |
       v
    fwrite()
       |
       v
    File System

This implementation is NOT an end-to-end zero-copy implementation.

It is a baseline implementation that can later be compared with an optimized data path.

---

# 24. Future Optimized Data Path

The planned optimized version of the project can investigate:

    Network
       |
       v
    XDP/eBPF
       |
       v
    AF_XDP
       |
       v
    Reusable / Shared Buffers
       |
       v
    Application
       |
       v
    io_uring
       |
       v
    Storage

The purpose is to investigate ways to reduce unnecessary data movement and processing overhead.

The current TCP implementation can therefore serve as the baseline for performance comparison.

---

# 25. Important Design Assumption

The current receiver assumes:

    Another process is already listening
    on the specified TCP port.

For example:

    Data Sender
         |
         | listen()
         |
         | :9000
         |
         v
    Receiver Child
         |
         | connect()
         |
         v
    TCP connection

The sender must send the complete file and then close the connection.

---

# 26. Simple Mental Model

The easiest way to remember the entire program is:

    1. Get username, port and filename.
             |
             v
    2. Check user's directory.
             |
             v
    3. Create destination file.
             |
             v
    4. Update data.txt.
             |
             v
    5. fork()
             |
             v
    6. Child connects to port.
             |
             v
    7. Child receives data.
             |
             v
    8. Child writes data to file.
             |
             v
    9. Sender closes connection.
             |
             v
   10. Child exits.
             |
             v
   11. Parent finishes.

---

# 27. One-Line Summary

The program is a file-storage manager that validates a user's directory and creates a destination file, then uses a separate child process to receive raw data over a TCP connection and store it in that file.
