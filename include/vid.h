#ifndef VID_H
#define VID_H
#include<stdio.h>
#include<string.h>
#include"memory.h"
class stream{
FILE *mpv=NULL;
public:
char *address=NULL;
stream(char *adr){
const char *display=getenv("WAYLAND_DISPLAY");
if (display!=NULL){
mpv=popen("mpv --vo=wayland -", "w");
}
else{
mpv=popen("mpv --vo=x11 -", "w");
}
if (mpv == NULL){
perror("mpv creation of pipe failed\n");
}
address=adr;
}
int stream_update(size_t n){
fwrite(address,1,n,mpv);
fflush(mpv);
return 1;
}
~stream(){
pclose(mpv);
}
};
class downloader{
FILE *fpd=nullptr;
int sizes=0;
quee A;
int *lo;
char *streamer=NULL;
void runner(FILE *fpd){
char c;
quee a;
a.a=0;
while(1){
a=read_space(streamer,a.a,*lo,sizes);
if(a.c=='\0')break;
else{
fprintf(fpd,"%c",a.b);
}
}
return ;
}
public:
/// downloader( input file name , pointer to upper bound of write,pointer to the space (char *)find_ptr)
downloader(char *name,int sized,int *stx,char *streaming_ptr){
fpd=fopen(name,"r");
if(fpd!=NULL){
printf("Entered name already exists aborting downloads...");
fclose(fpd);
}else{
fpd=fopen(name,"w");
}
sizes=sized;
lo=stx;
streamer=streaming_ptr;
}
int run(){
if(fpd!=NULL){
std::thread sr(&downloader::runner,this,fpd);
return 1;
}else{
return 0;
}
return 0;
}
~downloader(){
fclose(fpd);
}
};
#endif // VID_H
