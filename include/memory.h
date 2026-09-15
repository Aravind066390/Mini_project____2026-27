#ifndef MEMORY_H_INCLUDED
#define MEMORY_H_INCLUDED
#include<cstdio>
#include<sys/fcntl.h>
#include<sys/mman.h>
#include<unistd.h>
#include<thread>
#include<stdlib.h>
#include<string.h>
#include<mutex>
std::mutex m;
///status:Completely checked.
/**
--Return status for stack overflow of memory records is -1.
--Returns -2 when allocation in open fails.
--Returns -5 when the reuqest for creation already exists.
--add_space(size_of_new_mapped_memory) which returns the file descriptor of the opened memory map.
--Returns -3 if the remove_space function is called without calling add_space.
--Returns -4 if the reuqested memory space to be removed doesn't exists.
--Returns NULL in find_space if the address is not found.
*/
typedef struct memory_addr{
int addr;
char value[6];
void *loc;
int sizes;
struct memory_addr *next;
}memory_addr;
typedef struct quee{
int a,c;
char b;
}quee;
memory_addr *records=NULL,*head=NULL;
memory_addr * find_space(int s){//check finished.
memory_addr *temp=head,*backe=NULL;
if(temp==NULL){
//printf("temp is null\n");
    return NULL;
}
else{
char ch[5];
sprintf(ch,"%d",s);
//printf("\n");
while(temp!=NULL){
//printf("%s!=%s  ",temp->value,ch);
if(!strcmp(ch,temp->value)){
return temp;
}
backe=temp;
temp=temp->next;
}
}
//printf("\n");
return NULL;
}
int creat_space(int rec_size,int s,int state){//check finished.
/// create_space( int id_of_new_space,int size_of_new_space, int 0 if its create new shm and 1 if its open the existing shm.
char temp[5];
if(find_space(rec_size)!=NULL){
return -5;
}
if(records==NULL){
records=(memory_addr *)malloc(sizeof(memory_addr));
records->next=NULL;
head=records;
}else{
records->next=(memory_addr *)malloc(sizeof(memory_addr));
records=records->next;
records->next=NULL;
}
if(rec_size<0){
    return -1;
}
sprintf(temp,"%d",rec_size);
if(state==0){
records->addr=shm_open(temp,O_CREAT|O_RDWR,0666);
ftruncate(records->addr,s);
}else{
records->addr=shm_open(temp,O_CREAT|O_RDWR,0666);
}
if(records->addr==-1){
    return -2;
}
records->loc=(char *)mmap(
             NULL,
             s,
             PROT_READ|PROT_WRITE,
             MAP_SHARED,
             records->addr,
             0
             );
strcpy(records->value,temp);
records->sizes=s;
return records->addr;
}
int close_space(int s){//check finished.
memory_addr *temp=head,*backe=NULL;
if(temp==NULL){
    return -3;
}
else{
char ch[5];
sprintf(ch,"%d",s);
while(temp!=NULL&&strcmp(ch,temp->value)){
backe=temp;
temp=temp->next;
}
}
if(temp==NULL){
    return -4;
}else{
backe->next=temp->next;
munmap(temp->loc,temp->sizes);
close(temp->addr);
free(temp);
return 1;
}
}
quee read_space(char *s,int lower,int upper,int sizes){//check finished.
quee a;
a.c=1;
if(((lower+1)%sizes)==(upper%sizes)){
a.c=-1;
a.a=((lower)%sizes);
return a;
}else{
a.b=s[(lower%sizes)];
a.a=((lower+1)%sizes);
a.c=1;
}
return a;
}
quee write_space(char *s,int lower,int upper,int sizes,char ch){//check finished.
quee a;
a.c=1;
//fseek(fp,-1,SEEK_CUR);
//m.lock();
//printf("%d----%d\n",upper,lower);
if(((upper+1)%sizes)==(lower%sizes)){
a.a=upper%sizes;
a.c=-1;
//printf("mola\n");
}else{
s[(upper%sizes)]=ch;
a.a=((upper+1)%sizes);
a.c=1;
}
//printf("%d----%d\n",upper,lower);
//m.unlock();
return a;
}
int clear_space(){
//check finished.
memory_addr *a=head,*temp=NULL;
while(a!=NULL){
temp=a;
a=a->next;
munmap(temp->loc,temp->sizes);
close(temp->addr);
free(temp);
}
return 1;
}
void decouple_space(){///Just more of a useless stuff tooooooo risky to use if used use at your own risk.
memory_addr *a=head,*temp=NULL;
while(a!=NULL){
temp=a;
a=a->next;
free(temp);
}
}
void * find_space_ptr(int s){
memory_addr *p=find_space(s);
return p->loc;
}
int unlink_all(){///Delete the space permanently.
//check finished
memory_addr *a=head;
while(a!=NULL){
shm_unlink(a->value);
a=a->next;
}
return 1;
}
int unlink_one(int s){
memory_addr *temp=head,*backe=NULL;
if(temp==NULL){
    return -3;
}
else{
char ch[5];
sprintf(ch,"%d",s);
while(temp!=NULL&&strcmp(ch,temp->value)){
backe=temp;
temp=temp->next;
}
}
if(temp==NULL){
    return -4;
}else{
backe->next=temp->next;
shm_unlink(temp->value);
free(temp);
return 1;
}
return 1;
}
#endif // MEMORY_H_INCLUDED
