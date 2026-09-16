#include<stdio.h>
int main(){
char instruction[]="mkdir APP_DATA_HYLINK";
system(instruction);
char instruction2[]="mkdir APP_DATA_HYLINK/Downloads";
system(instruction2);
char instruction3[]="mkdir APP_DATA_HYLINK/Data";
system(instruction3);
FILE *fp=fopen("APP_DATA_HYLINK/Data/STRT.txts","w");
fprintf(fp,"NULL but empty a tracker system");
fclose(fp);
fp=fopen("APP_DATA_HYLINK/Downloads/STRT.txts","w");
fprintf(fp,"NULL but empty a tracker system");
fclose(fp);
return 0;
}
