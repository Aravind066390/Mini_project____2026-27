#ifndef SIMPLE_SOCKET_H
#define SIMPLE_SOCKET_H
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <string>
class Socket{
    int fd = -1;
    bool send_all(const void *data,size_t size) {
        const char *ptr = (const char *)data;
        while (size > 0) {
            ssize_t n = send(fd, ptr, size, 0);
            if (n <= 0)
                return false;

            ptr += n;
            size -= n;
        }
        return true;
    }
    bool recv_all(void *data, size_t size) {
        char *ptr = (char *)data;
        while (size > 0) {
            ssize_t n = recv(fd, ptr, size, 0);
            if (n <= 0)
                return false;
            ptr += n;
            size -= n;
        }
        return true;
    }

public:
    Socket() {}
    ~Socket() {
        close_socket();
    }
    // Client connection
    bool connect_to(const char *ip, int port) {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
            return false;
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        if (inet_pton(AF_INET, ip, &addr.sin_addr) <= 0)
            return false;
        if (connect(fd, (sockaddr *)&addr, sizeof(addr)) < 0)
            return false;
        return true;
    }
    // Server
    bool listen_on(int port, int backlog = 5) {
        fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
            return false;
        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);
        if (bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0)
            return false;
        if (listen(fd, backlog) < 0)
            return false;
        return true;
    }
    // Accept client
    Socket accept_client() {
        Socket client;
        client.fd = accept(fd, nullptr, nullptr);
        return client;
    }
    // Send raw data
    bool put(const void *data, size_t size) {
        return send_all(data, size);
    }
    // Receive raw data
    bool get(void *data, size_t size) {
        return recv_all(data, size);
    }
    // Send file
    bool put_file(const char *filename) {
        FILE *fp = fopen(filename, "rb");
        if (!fp)
            return false;
        // Get file size
        fseek(fp, 0, SEEK_END);
        uint64_t size = ftell(fp);
        rewind(fp);
        // Send file size first
        if (!send_all(&size, sizeof(size))) {
            fclose(fp);
            return false;
        }
        char buffer[8192];
        while (size > 0) {
            size_t chunk = size > sizeof(buffer)? sizeof(buffer): size;
            size_t n = fread(buffer, 1, chunk, fp);
            if (n != chunk) {
                fclose(fp);
                return false;
            }
            if (!send_all(buffer, n)) {
                fclose(fp);
                return false;
            }
size -= n;
        }

        fclose(fp);
        return true;
    }
    // Receive file
    bool get_file(const char *filename) {
        FILE *fp = fopen(filename, "wb");
        if (!fp)
            return false;
        uint64_t size;
        // Receive file size
        if (!recv_all(&size, sizeof(size))) {
            fclose(fp);
            return false;
        }
        char buffer[8192];
        while (size > 0) {
            size_t chunk=(size>(sizeof(buffer)))?sizeof(buffer):size;
            if (!recv_all(buffer, chunk)) {
                fclose(fp);
                return false;
            }
            fwrite(buffer, 1, chunk, fp);
            size -= chunk;
        }
        fclose(fp);
        return true;
    }
    void close_socket() {
        if (fd >= 0) {
            close(fd);
            fd = -1;
        }
    }
};
#endif
///Author --Aravind
