#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h> // memcpy memset
#include <sys/socket.h>
#include <netinet/in.h> // sockaddr
#include <unistd.h> //read write close
#include <arpa/inet.h> //inet_pton


char* translate_header(char* request){

}

int main(){

    int sock = socket(AF_INET,SOCK_STREAM,0);
    int opt = 1;
    setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    setsockopt(sock,SOL_SOCKET,SO_REUSEPORT,&opt,sizeof(opt));

    char* buffer[4096];
    memset(buffer,0,4096);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;

    inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);
    addr.sin_port = htons(8080);

    ssize_t size = sizeof(addr);
    bind(sock,(struct sockaddr*)&addr,size);

    listen(sock,10);

    ssize_t rec_size;
    struct sockaddr_in recived;
    socklen_t s = sizeof(sock);

    int rec_sock = accept(sock,(struct sockaddr*)&recived,&s);
    rec_size = read(rec_sock,buffer,4095);
    printf("%s\n",buffer);
}