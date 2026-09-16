#include"network.h"
#include"logs.h"
int route(Strings *s1){
int size=0;
char strtmp[100];
strcpy(strtmp,s1->get_str());
if(!strcmp(strtmp,"POST")){
start_post_sequence(s1);
}else if(!strcmp(strtmp,"GET")){
start_get_sequence(s1);
}
else if(!strcmp(strtmp,"SIGN_UP")){
///SIGN_UP IS FOR NEW USERS
create_new_account(s1);
}else if(!strcmp(strtmp,"SIGN_IN")){
///SIGN_IN IS FOR NEW SESSION FOR ALREADY EXISTING USERS OR SIGNED_UP USERS.
new_session(s1);
}
return 1;
}
int main(int n,char **str){
int i=0;
Strings *s1=new Strings(str,n);
while(i<n){
i+=route(s1);
}
return 0;
}
