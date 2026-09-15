#ifndef BUFFER_H_INCLUDED
#define BUFFER_H_INCLUDED
#include"memory.h"
#include<stdio.h>
typedef struct Data_pack{
pid_t owner;
void *ptr;
}Data_pack;
pid_t *space=NULL;
int generate_system(){
creat_space(15,2048,0);
space=(pid_t *)find_space_ptr(15);
space[0]=1;
return 1;
}
int start_system(){
creat_space(15,2048,1);
space=(pid_t *)find_space_ptr(15);
return 1;
}
int destroy_system(){
unlink_one(15);
return 1;
}
int clear_system(){
close_space(15);
return 1;
}
int validity(Data_pack *A,int id){
if(space==NULL){
return 0;
}
if(space[id]==A->owner){
return 1;
}
return 0;
}
int revalidate(Data_pack *A,int id){
if(space==NULL){
return 0;
}
space[id]=A->owner;
return 1;
}
int add_validity(Data_pack *A,int id){
if(space==NULL){
return 0;
}
space[space[0]++]=A->owner;
return space[0]-1;
}
class data_pack{
Data_pack *A=NULL;
public:
int res_check,id,destroy=1,idm;
template <typename T>
data_pack(int id,int sizes,int state){
A=(Data_pack *)malloc(sizeof(Data_pack));
A->owner=getpid();
this->id=id;
res_check=creat_space(id,sizes,state);
A->ptr=find_space(id);
if(state==0){
idm=add_validity(A,id);
}
}
int int_write(int valor){
if(validity(A,id)){
*((int*)(A->ptr))=valor;
return 1;
}
return 0;
}
int float_write(float valor){
if(validity(A,id)){
*((float*)(A->ptr))=valor;
return 1;
}
return 0;
}
int double_write(double valor){
if(validity(A,id)){
*((double*)(A->ptr))=valor;
return 1;
}
return 0;
}
int char_write(int valor){
if(validity(A,id)){
*((char*)(A->ptr))=valor;
return 1;
}
return 0;
}
int int_read(int locs){
if(validity(A,idm)){
return  ((int *)A->ptr)[locs];
}
return 0;
}
int float_read(int locs){
if(validity(A,idm)){
return ((float *)A->ptr)[locs];
}
return 0;
}
int double_read(int locs){
if(validity(A,idm)){
return ((double *)A->ptr)[locs];
}
return 0;
}
char char_read(int locs){
if(validity(A,idm)){
return ((char *)A->ptr)[locs];
}
return 'A';
}
int take_validity(){
if(!validity(A,idm)){
revalidate(A,idm);
}
return 1;
}
int destroy_pack(){
unlink_one(id);
destroy=0;
return 1;
}
~data_pack(){
if(destroy)
close_space(id);
}
};
#endif
