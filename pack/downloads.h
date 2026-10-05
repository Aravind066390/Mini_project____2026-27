/**
--Returns 0 when the check to verify the existance of a file that doesnt exist is created and returned with 0.
--open_download(download_address|name)
--close_download(download_address|name)
--Returns -12 when the file name is above 126 bytes of size due to corruption of memory as the buffer for the copying of name is just of 127 bytes size.
--Returns -22 when the requested file from donwloads not found.
--Returns -23 when the reuqested files address is returned but when trying to access it the program fails and returns -23.s
*/
#ifndef DOWNLOADS_H_INCLUDED
#define DOWNLOADS_H_INCLUDED
#include<string.h>
#include<semaphore.h>
#include<stdio.h>
#include"memory.h"
#include<sys/stat.h>
struct stat st;
#define CREATE 0
#define RDWR 1
#define KB 1024
#define MB (KB*1024)
#define GB (MB*1024)
sem_t *sem=sem_open("/mysem",O_CREAT,0666,1);
char * give_addr(char *s){
char *temp=(char *)malloc(1000);
start://this is to re-run the system to return a guarentted address in the download space.
strcpy(temp,"downloads/");
strcat(temp,s);
FILE *fp=fopen(temp,"r");
if(fp==NULL){
if(stat("downloads",&st)==0&&S_ISDIR(st.st_mode)){
return NULL;
}else{
system("mkdir downloads");
goto start;
}
}else{
fclose(fp);
}
return temp;
}
int write_download(char *addr,char *data){
memory_addr *ptr=find_space(0);
FILE *fp=NULL;
if(((int *)ptr->loc)[1]==0){
fp=fopen(addr,"w");
}else{
fp=fopen(addr,"a+");
}
fprintf(fp,"%s",data);
((int *)ptr->loc)[1]++;
fclose(fp);
return 0;
}
int check(char *s){//check finished.
FILE *fp=fopen(s,"r");
if(fp==NULL){
fp=fopen(s,"w");
fclose(fp);
return 0;
}
fclose(fp);
return 1;
}
int open_download(char *s){//check finished.
char rec[]="records";
check(rec);
FILE *fp=fopen(rec,"a+");
fprintf(fp,"%99s\n",s);
fclose(fp);
return 0;
}
int close_download(char *s){//close a running record from the system through records.
//check finished.
char rec[]="records";
if(check(rec)==0){
return -1;
}
char t[1000];
char nxt[]="mv records temp";
strcpy(t,nxt);
system(t);
strcpy(t,"NULL");
FILE *fp=fopen("temp","r");
FILE *fps=fopen("records","w");
while((fscanf(fp,"%99s",t))!=EOF){
if(strcmp(s,t)){
fprintf(fps,"%99s\n",t);
}
}
fclose(fp);
fclose(fps);
char tes[]="temp";
remove(tes);
return 0;
}
char * pop_download(){
//check finished.
//Pop the first element from the system.
char rec[]="records";
FILE *fp=fopen(rec,"r");
if(check(rec)==0){//To check if the records are full or empty.
return NULL;
}
char *t=(char *)malloc(1000);
while(1){
if(EOF==fscanf(fp,"%99s",t)){
return NULL;
}
if(give_addr(t)==NULL){
break;
}
}
fclose(fp);
return t;
}
int push_download(char *s){//same as open downloads.
FILE *fp=fopen("records","r");
if(fp==NULL){
fp=fopen("records","w");
}
fclose(fp);
fp=fopen("records","a+");
fprintf(fp,"%99s\n",s);
fclose(fp);
return 0;
}
int begin_state(short int i){
//check finished.
//here begin_state(CREATE|RDWR);
//To start everything we first need to initialize this to create a vm to act as a shm between jvm and c.
int N=10;
if(i==0){
if(-5==creat_space(0,(N*KB+128),CREATE)){
printf("error");
return -5;
}
if(creat_space(1,KB,CREATE)==-5){
printf("error\n\n");
return -5;//error not able to allocate space.
}
return 1;
}else{
int N=10;
if(-5==creat_space(0,(N*KB+128),RDWR)){
printf("error");
return -5;
}
if(creat_space(1,KB,RDWR)==-5){
printf("error\n\n");
return -5;//error not able to allocate space.
}
return 1;
}
sem_init(sem,1,1);
}
char *get_data(char *s){
//check finished.
char *ch=(char *)malloc(10*KB);//here change this also when you change N.
memory_addr *ptr=find_space(0);
((int *)ptr->loc)[0]=25;
if(sizeof(s)<122)//one extra space of safety no tech reasons here.
strcpy((((char *)ptr->loc)+4),s);
else
return NULL;
while((((int *)ptr->loc)[0])!=12){};
strcpy(ch,((char*)ptr->loc)+128);
((int *)ptr->loc)[0]=24;
return ch;
}
int download_handle(){
char *ch,*addr,*data,a;
while(1){
ch=pop_download();
if(ch==NULL){
break;
}else{
addr=give_addr(ch);
data=&a;
while(data!=NULL){
free(data);
data=get_data(ch);
write_download(addr,data);
}
close_download(ch);
}
free(ch);
free(addr);
}
return 0;
}
int listen_downloads(){
memory_addr *ptr=find_space(0);
char a[130];
if(((int *)ptr->loc)[0]==128)return 128;
if(((int *)ptr->loc)[0]==100){
strcpy(a,((char*)ptr->loc)+4);
a[130]='\0';
push_download(a);
download_handle();
}
return 0;
}
int delete_download(char *s){//check finished.
char *ch=give_addr(s);
if(ch!=NULL){
return -1;
}
remove(ch);
free(ch);
return 0;
}
//int upper_bound_space=0,lower_bound_space=0;
//read_to_quee means when a donwload vedio is picked then it is loads the data in that file into a circular quee.
int read_to_quee(FILE *fp,int *lower_bound_space,int *upper_bound_space,memory_addr *pr){//check finished.
char ch;
quee ptr;
ptr.c=1;
if(fscanf(fp,"%c",&ch)==EOF){//if the file has reached EOF then we just exit.
return 0;
}
ptr=write_space((char *)pr->loc,*lower_bound_space,*upper_bound_space,KB,ch);//here kb.
*upper_bound_space=ptr.a;
if(ptr.c==-1){
fseek(fp,-1,SEEK_CUR);
return -1;
}
//printf("%d\n",ptr.c);
return 1;
}
int read_from_donwloads(char *s,int *lower_bound_space,int *upper_bound_space){//check finished.
s=give_addr(s);
if(s==NULL)return -22;
FILE *fp=fopen(s,"r");
if(fp==NULL)return -23;
int i=1;
fseek(fp,0,SEEK_SET);
memory_addr *pr=find_space(1);
while(i){
m.lock();
sem_wait(sem);
i=read_to_quee(fp,lower_bound_space,upper_bound_space,pr);
sem_post(sem);
m.unlock();
if(i==-1){
//printf("%d----%d\n",*upper_bound_space,*lower_bound_space);
std::this_thread::sleep_for(std::chrono::milliseconds(10));
}
}
fclose(fp);
return 0;
}
char write_from_downloads(int *lower_bound_space,int *upper_bound_space){//check finished.
memory_addr *pr=find_space(1);
quee ptr;
jump:
m.lock();
sem_wait(sem);
ptr=read_space((char *)pr->loc,*lower_bound_space,*upper_bound_space,KB);//here use kb.
*lower_bound_space=ptr.a%KB;
if(ptr.c==-1){
//printf("%d----%d\n",*upper_bound_space,*lower_bound_space);
sem_post(sem);
m.unlock();
std::this_thread::sleep_for(std::chrono::milliseconds(10));
goto jump;
};
sem_post(sem);
m.unlock();
return ptr.b;
}

/**
flow of data in download system--->
ptr->loc[0]=100 it is to check or verify the thing that the vedio request is been created done by JVM to inform c.
ptr->loc[0]=25 it is to tell the JVM that c is ready.
ptr->loc[0]=12 JVM telling c that ok now you can read this start reading (JVM->c).
ptr->loc[1]=? this is the total number of segments already read.
ptr->loc[2-128]=? The name of the file.
ptr->loc[128-?]=? This stores the complete vedio data.
ptr->loc[0]=128 A special signal to stop the complete process and just exit fully.
*/
/**
CODE STRUCTURE:-

begin_state();
download_handle();
while(1){
if(listen_downloads()==128)break;
}
close_download();
*/
#endif // DOWNLOADS_H_INCLUDED
