#include<stdlib.h>
int main(){
system("g++ receive.c -o receive");//1
system("g++ sender.c -o sender");//2
system("g++ client.c -o client");//3
system("g++ init_server.c -o init_server");//4
system("g++ tes.c -o tes");//5
system("g++ strt.c -o strt");//6
system("g++ run_server_send.c -o run_server_send");//7
system("g++ run_server_get.c -o run_server_get");//8
system("g++ server_api.c -o server_api");//9
return 0;
}
