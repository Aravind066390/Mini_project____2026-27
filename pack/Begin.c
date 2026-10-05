#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include<stdlib.h>
int main(void) {
    const char *url = "http://127.0.0.1:8080";

    pid_t pid = fork();
    if (pid == 0) {
        execlp("firefox", "firefox", url, (char *)NULL);
        perror("execlp");
        _exit(127);
    } else if (pid < 0) {
        perror("fork");
        return 1;
    }else{
    system("python3 webui.py ./server_api 8080");
    }
}
