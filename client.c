#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>



int main(){


    int s = socket(AF_INET,SOCK_STREAM,0);
    char buffer[4096];
    struct sockaddr_in addr;
    
    addr.sin_family = AF_INET;
    inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
    addr.sin_port = htons(8080);
    memset(buffer,0,4096);
    
    scanf("%s",buffer);

    int conn = connect(s,(struct sockaddr*)&addr,sizeof(addr));

    send(s,&buffer,sizeof(buffer),0);
    close(s);
    return 1;
    
}