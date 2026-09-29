#ifndef THREAD
#define THREAD
#include<thread>
#include<iostream>
int next_id=0;
class pool{
std::thread t1;
public:
template <typename T,typename... Args>
int submit(T value,Args... vals){
t1=std::thread(value,vals...);
return 0;
}
~pool(){
if(t1.joinable()){
t1.join();
}
}
};
typedef struct promise{
promise *next;
int promise_ID;
pool *value;
}promise;
promise *head=NULL,*temp=NULL;
class make_promise{
private:
promise* generate_promise(){
promise *pro=(promise *)malloc(sizeof(promise));
if(temp==NULL){
temp=pro;
head=pro;
}else{
temp->next=pro;
temp=pro;
}
pro->next=NULL;
return pro;
}
int init_promise(promise *A1,int id,pool *A){
A1->promise_ID=id;
A1->value=A;
return 1;
}
public:
make_promise(int id,pool *A){
promise *A1=generate_promise();
init_promise(A1,id,A);
}
};
template <typename T,typename... Args>
promise* async(T value1,Args... value2){
pool *A=new pool();
A->submit(value1,value2...);
make_promise *A1=new make_promise(next_id,A);
delete A1;
next_id+=1;
return temp;
}
int await(promise *A){
promise *beg=head,*backe=NULL;
while(beg!=NULL){
if(beg==A){
delete A->value;
if(backe!=NULL){
backe=beg->next;
}
free(beg);
return 0;
}
backe=beg;
beg=beg->next;
}
return 1;
}
int calc(int n){
int x=0,l=0;
while(n>0){
if(n%5==0){
l+=1;
}
n-=l;
x++;
}
return x;
}
#endif // THREAD
