#include<stdio.h>
#include<string.h>
#include"memory.h"
typedef struct{
char **value;
float **flt2;
int g,x;
}sql_data;
char strt[1024];
class sqlr{
public:
int m=0,n=0,l=0;
sql_data *A;
sqlr(int x,int y){
m=x;
n=y;
A=(sql_data *)malloc(sizeof(sql_data));
A->g=0;
A->x=10;
A->flt2=(float **)malloc(sizeof(float *)*10);
A->value=(char **)malloc(x*sizeof(char *));
for(int i=0;i<x;i++){
A->value[i]=(char *)malloc(y);
}
}
int add(char *s){
strcpy(A->value[l++],s);
return 1;
}
int view(){
for(int i=0;i<l;i++){
printf("%s\n",A->value[i]);
}
return 1;
}
~sqlr(){
for(int i=0;i<m;i++){
free(A->value[i]);
}
}
};
int start_sql(char *st){
strcpy(strt,st);
creat_space(21,1024*1024,0);
return 1;
}
int end_sql(){
unlink_all();
return 1;
}
sqlr* ask_sql(char *st){
char *s=(char *)find_space_ptr(21);
if(s==NULL){
return NULL;
}
char str[1024*1024];
strcpy(str,strt);
strcat(str,"  ");
strcat(str,st);
strcat(str," > /dev/shm/21");
system(str);
int i=0,maximum=0,temp=0,js=0;
while(s[i]!='\0'){
while(s[i]!='\n'&&s[i]!='\0'){
temp+=1;
i+=1;
}
js+=1;
if(maximum<temp){
maximum=temp;
}
temp=0;
if(s[i]=='\0')break;
i+=1;
}
sqlr *vals=new sqlr(js+1,maximum+1);
i=0;
int ns=0;
while(s[i]!='\0'){
while(s[i]!='\n'&&s[i]!='\0'){
str[ns++]=s[i++];
}
str[ns]='\0';
ns=0;
vals->add(str);
if(s[i]=='\0')break;
i+=1;
}
return vals;
}
int give_sql(char *s){
printf("%s",s);
char str[1024*1024];
strcpy(str,strt);
strcat(str,"  ");
strcat(str,s);
system(str);
return 1;
}
float extract_numbers(char *s,int val){
int i=0,st=1,t=0;
float fl=0,mt=0;
float ts=1;
while(s[i]!='\0'){
if(s[i]==' '&&s[i]!='\0'){
while(s[i]!=' '){
i+=1;
}
if(s[i]=='\0')
val-=1;
}
if(val==1){
while(s[i]!='\0'&&s[i]!=' '){
if(s[i]=='+'){
i+=1;
}else if(s[i]=='-'){
i+=1;
st=-1;
}
if(s[i]=='.'){
t=1;
i+=1;
}
if(t){
ts/=10;
mt+=((s[i]-'0')*ts);
}else{
fl+=s[i]-'0';
fl*=10;
}
i+=1;
if(s[i]==' '||s[i]=='\0'){
if(fl>0){
fl/=10;
}
return (mt+fl)*st;
}
}
i+=1;
}
i+=1;
}
return -1;
}
float* inp_float(sqlr *A,int y){
float *g=(float *)malloc(sizeof(float )*A->l);
for(int i=0;i<A->l;i++){
g[i]=extract_numbers(A->A->value[i],y);
}
if(A->A->g>=(A->A->x)){
A->A->x+=10;
A->A->flt2=(float **)realloc(A->A->flt2,sizeof(float *)*(A->A->x));
}
A->A->flt2[A->A->g++]=g;
return g;
}
