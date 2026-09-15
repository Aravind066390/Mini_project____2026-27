#ifndef LOGS_H
#define LOGS_H
#include<stdio.h>
#include<time.h>
typedef struct data{
FILE *fp;
}data;
class log{
data *a=NULL;
public:
log(){
a=(data *)malloc(sizeof(data));
a->fp=fopen("data.log","a");
if(a->fp==NULL){
a->fp=fopen("data.log","w");
}
}
int log_write(char *message){
time_t now;
time(&now);
fprintf(a->fp,"%s\n%s\n",ctime(&now),message);
return 1;
}
~log(){
free(a);
}
};
#endif
