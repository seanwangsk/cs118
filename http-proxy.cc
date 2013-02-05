/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

int main (int argc, char *argv[])
{
  // command line parsing
  socklen_t len;
  struct sockaddr_in server; 
  int sock_desc = socket(AF_INET, SOCK_STREAM,0);
  server.sin_family = AF_INET;
  server.sin_addr.s_addr = inet_addr("127.0.0.1");
  server.sin_port = 9989;
  struct sockaddr_in client;
  int k = bind(sock_desc, (struct sockaddr*)&server, sizeof(server));
  printf("result for binding is %d\n",k);
  len = sizeof(client);
  k = listen(sock_desc,20);
  printf("result for listen is %d\n",k);
  int temp_sock_desc = accept(sock_desc, (struct sockaddr*)&client, &len);
  while(1){
  	char buf[512];
  	k = recv(temp_sock_desc,buf,512,0);
	if(strcmp(buf,"exit")==0){
		break;
	}
	if(k>0){
		cout<<buf<<endl;
	}
  }
  close(temp_sock_desc);
  close(sock_desc);
  return 0;
}
