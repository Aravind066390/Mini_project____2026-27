#ifndef PROCESS_H_INCLUDED
#define PROCESS_H_INCLUDED
#include<unistd.h>
#include<string.h>
#include<signal.h>
#include<stdio.h>
#include<sys/wait.h>
#include<semaphore.h>
sem_t sems;
typedef struct proc{
int i;
pid_t pid;
char f_name[124];
struct proc *next;
}proc;
proc *head_tracker=NULL,*tail_tracker=NULL;
int add_track(int a,char *s){
proc *proces=(proc *)malloc(sizeof(proc));
if(proces==NULL)return 101;
if(tail_tracker==NULL){
head_tracker=proces;
tail_tracker=proces;
tail_tracker->next=NULL;
tail_tracker->i=a;
strcpy(tail_tracker->f_name,s);
}else{
tail_tracker->next=proces;
tail_tracker=tail_tracker->next;
tail_tracker->i=a;
strcpy(tail_tracker->f_name,s);
tail_tracker->next=NULL;
}
pid_t pids;
if(0==(pids=fork())){
execl(s,"ls","-l",NULL);
}else if(pids>0){
tail_tracker->pid=pids;
}else{
proc *news=head_tracker,*back_y=NULL;
while(head_tracker->next!=NULL){
back_y=news;
news=news->next;
}
free(tail_tracker);
tail_tracker=back_y;
free(tail_tracker);
return 107;
}
return 0;
}
int new_track(){
proc *next_trac=head_tracker;
int last=-1;
while(next_trac!=NULL){
last=next_trac->i;
next_trac=next_trac->next;
}
return last+1;
}
int process_create(char *s){
return add_track(new_track(),s);
}
int process_kill(char *s){
if(head_tracker==NULL)return 105;
proc *point=head_tracker,*back_tr=NULL;
while(point!=NULL){
if(!strcmp(point->f_name,s)){
if(0==kill(point->pid,SIGTERM)){
if(point==head_tracker){
head_tracker=head_tracker->next;
}else{
back_tr->next=point->next;
}
if(point->next==NULL){
tail_tracker=back_tr;
}
free(point);
}
return 0;
}
back_tr=point;
point=point->next;
}
return 102;
}
int shutdown(){
if(head_tracker==NULL)return 105;
int i=0;
proc *point=head_tracker,*back_tr=NULL;
while(point!=NULL){
back_tr=point;
head_tracker=head_tracker->next;
point=point->next;
kill(back_tr->pid,SIGTERM);
free(back_tr);
if(point==tail_tracker){
kill(point->pid,SIGTERM);
free(point);
head_tracker=NULL;
tail_tracker=NULL;
return 0;
}
}
return 103;
}
void view_proc(){
proc *v=head_tracker;
if(v==NULL){
printf("\n(nul)\t(nul)\t(nul)\t(nul)\n");
return ;
}
while(v!=NULL){
printf("\n%d\t%s\t%p\t%d\n",v->i,v->f_name,v->next,(int)v->pid);
v=v->next;
}
return ;
}
int process_add(pid_t a,char *s){
///Add a new listed process into the system.
int ats=new_track();
proc *proces=(proc *)malloc(sizeof(proc));
if(proces==NULL)return 101;
proces->pid=a;
strcpy(proces->f_name,s);
if(tail_tracker==NULL){
head_tracker=proces;
tail_tracker=proces;
tail_tracker->next=NULL;
tail_tracker->i=ats;
strcpy(tail_tracker->f_name,s);
}else{
tail_tracker->next=proces;
tail_tracker=tail_tracker->next;
tail_tracker->i=ats;
strcpy(tail_tracker->f_name,s);
tail_tracker->next=NULL;
}
return 0;
}///check this function before using please.
int process_init(int x){///incomplete
sem_init(&sems,0,1);
sem_wait(&sems);
creat_space(x,4096,1);///one means load a existing space.
memory_addr *t=find_space(x);
pid_t d;
char s[99];
sem_t *sent=(sem_t *)t->loc;
sem_init(sent,0,1);
sem_wait(sent);
t->loc=((char *)t->loc)+sizeof(sem_t);
while(1){///this is to read data from shm but
d=*(pid_t *)t->loc;
t->loc=((char *)t->loc)+sizeof(pid_t);
if(d==0)break;
strcpy(s,(char *)t->loc);
t->loc=((char *)t->loc)+strlen((char *)t->loc)+1;
///this is where i need to push data into my local storage.
if(d!=getpid()){
process_add(d,s);
}
}
sem_post(&sems);
sem_post(sent);
sem_destroy(&sems);
sem_destroy(sent);
return 0;
}
int update(int x){
char ch[100];
sprintf(ch,"%d",x);
shm_unlink(ch);
creat_space(x,4096,0); ///zero means create a new space.
sem_t *gt=(sem_t *)find_space_ptr(x);
memory_addr *t=find_space(x);
sem_init(&sems,0,1);
sem_wait(&sems);
sem_init(gt,1,1);
sem_wait(gt);
proc *te=head_tracker;
if(te==NULL)return 129;
t->loc=((char *)t->loc)+sizeof(sem_t);
while(te!=NULL){
*((pid_t *)t->loc)=te->pid;
//printf("INT:%d and %d\n",t->loc,te->pid);
t->loc=((char *)t->loc)+sizeof(pid_t);
strcpy((char *)t->loc,te->f_name);
t->loc=((char *)t->loc)+strlen(te->f_name)+1;
te=te->next;
}
sem_post(gt);
sem_destroy(gt);
sem_post(&sems);
sem_destroy(&sems);
return 0;
}
int block(char *s,int stats){///It takes 0 if just block and 1 if block and after finishing even perform process_kill
if(head_tracker==NULL)return 121;
int status;
if(!strcmp("*",s)){
proc *v=head_tracker;
while(v!=NULL){
while(waitpid(v->pid,&status,0)==0);
v=v->next;
}
if(stats){
shutdown();
}
}else{
proc *v=head_tracker;
while(v!=NULL){
if(!strcmp(s,v->f_name)){
while(waitpid(v->pid,&status,0)==0);
if(stats){
process_kill(v->f_name);
}
return 0;
}
v=v->next;
}
}
return 111;
}
int shutdown_dead(){
///Allows to kill the dead processes from the table;
if(head_tracker==NULL)return 121;
int status,i=0;
proc *v=head_tracker;
while(v!=NULL){
if(waitpid(v->pid,&status,0)!=0){
process_kill(v->f_name);
i++;
}
v=v->next;
}
return i;
}
#endif // PROCESS_H_INCLUDED
