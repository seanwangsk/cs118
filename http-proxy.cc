/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include "http-request.h"
#include <netdb.h>

#define PORT 19989
#define BUFFERSIZE 512

using namespace std;

int create_tcp_socket(){
    int sock;
    if((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP))<0){
        cerr<<"ERROR on accept"<<endl;
        exit(1);
    }
    return sock;
}

char * get_ip(const char * host){
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


int main (int argc, char *argv[])
{
  // command line parsing
  socklen_t len;
  int sock_desc = create_tcp_socket();

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
  	if(recv(temp_sock_desc,buf,BUFFERSIZE,0)<0){
		cerr<<"ERROR on reading data"<<endl;
		exit(1);
	}

	HttpRequest req;

	const char *buf3 = "GET http://www.google.com:80/index.html/ HTTP/1.0\r\nContent-Length:80\r\nIf-Modified-Since: Sat, 29 Oct 1994 19:43:31 GMT\r\n\r\n";
	req.ParseRequest(buf3, BUFFERSIZE);
	size_t size = req.GetTotalLength();
      char* ip = get_ip((req.GetHost()).c_str());
      char buf2[size];
      req.FormatRequest(buf2);
      int sock_fetch = create_tcp_socket();
      struct sockaddr_in client;
      client.sin_family = AF_INET;
      client.sin_addr.s_addr = inet_addr(ip);
      client.sin_port = req.GetPort();
      connect(sock_fetch, (struct sockaddr*)&client, sizeof(client));
      send(sock_fetch, buf2, size, 0);
      
      char buf4[BUFFERSIZE];
      if(recv(sock_fetch,buf4,BUFFERSIZE,0)<0){
          cerr<<"ERROR on reading data"<<endl;
          exit(1);
      }
	cout<<buf4<<endl;
      
      send(temp_sock_desc, buf4, BUFFERSIZE, 0);
  }
  close(temp_sock_desc);
  close(sock_desc);
  return 0;
}


