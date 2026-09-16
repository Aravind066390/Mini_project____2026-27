#include<stdio.h>
#include"logs.h"
int check(){
FILE *fp=NULL;
fp=fopen("APP_DATA_HYLINK/Data/STRT.txts","r");
if(fp==NULL){
return 1;
}
fclose(fp);
fp=NULL;
fp=fopen("APP_DATA_HYLINK/Downloads/STRT.txts","r");
if(fp==NULL){
return  1;
}
return 0;
}
int main(int n,char **s){
if(check()){
system("./app_init");
}

return 1;
}
