/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

#define PORT 19989
#define BUFFERSIZE 512

using namespace std;

int main (int argc, char *argv[])
{
  // command line parsing
  socklen_t len;
  int sock_desc = socket(AF_INET, SOCK_STREAM,0);
  if(sock_desc < 0){
  	cerr<<"ERROR on opening the socket";
  }


  struct sockaddr_in serv_addr; 
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;//inet_addr("127.0.0.1");
  serv_addr.sin_port = htons(PORT);
  if(bind(sock_desc, (struct sockaddr*)&serv_addr, sizeof(serv_addr))<0){
 	 cerr<<"ERROR on binding"<<endl;
	 exit(1);
  }

  if(listen(sock_desc,20)<0){
  	cerr<<"ERROR on listening"<<endl;
	exit(1);
  }

  struct sockaddr_in cli_addr;
  len = sizeof(cli_addr);
  int temp_sock_desc = accept(sock_desc, (struct sockaddr*)&cli_addr, &len);
  if(temp_sock_desc < 0){
  	cerr<<"ERROR on accept"<<endl;
	exit(1);
  }
  while(1){
  	char buf[BUFFERSIZE];
  	if(recv(temp_sock_desc,buf,sizeof(buf),0)<0){
		cerr<<"ERROR on reading data"<<endl;
		exit(1);
	}
	if(strcmp(buf,"exit")==0){
		break;
	}
	cout<<buf<<endl;
  }
  close(temp_sock_desc);
  close(sock_desc);
  return 0;
}
