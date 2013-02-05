//TCP CLIENT
#include<sys/socket.h> 
#include<netinet/in.h> 
#include <arpa/inet.h>
#include<stdio.h> 
#include<string.h> 
#include<stdlib.h>
#include <arpa/inet.h>
//#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <netdb.h>

char* get_ip(char* host);

main()
{ 

    char buf[100]; 
    struct sockaddr_in client; 
    int sock_desc,k; 
    sock_desc = socket(AF_INET,SOCK_STREAM,0);
    memset(&client,0,sizeof(client)); 
    client.sin_family = AF_INET; 
    client.sin_addr.s_addr = inet_addr("127.0.0.1"); 
    client.sin_port = 19989; 
    k = connect(sock_desc,(struct sockaddr*)&client,sizeof(client)); 
    while(1)
    {     
        gets(buf);     
        k = send(sock_desc,buf,100,0);     
        if(strcmp(buf,"exit")==0)         
            break; 
    } 
    close(sock_desc); 
    return 0; 
}

char * get_ip(char * host){
	struct hostent *hent;
	int iplen = 15;
	char *ip = (char*) malloc(iplen+1);
	memset(ip,0,iplen+1);
	if((hent = gethostbyname(host))==NULL){
		printf("Can't find the hostname");
		exit(1);
	}
	if(inet_ntop(AF_INET, (void *)hent->h_addr_list[0],ip,iplen)==NULL){
		printf("Can't resolve host");
		exit(1);
	}
	return ip;
}

