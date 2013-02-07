/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <iostream>
#include <sys/socket.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>

#include "http-request.h"
#include "http-response.h"

#define _DEBUG 1

#ifdef _DEBUG
#include <iostream>
#define TRACE(x) std::cout << x << endl;
#else
#define TRACE(x) 
#endif // _DEBUG


#define PORT 19989
#define BUFFERSIZE 65535

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
  TRACE("bind successful")
  if(listen(sock_desc,20)<0){
  	cerr<<"ERROR on listening"<<endl;
	exit(1);
  }
  TRACE("listen succesful")
  struct sockaddr_in cli_addr;
  len = sizeof(cli_addr);
  int temp_sock_desc = accept(sock_desc, (struct sockaddr*)&cli_addr, &len);
  if(temp_sock_desc < 0){
  	cerr<<"ERROR on accept"<<endl;
	exit(1);
  }
  TRACE("accept successful");
  //while(1){
    string buf_data;
  	char buf_temp[BUFFERSIZE];
    const char * data;
    
    ssize_t size_recv;
    while((size_recv = recv(temp_sock_desc,buf_temp,BUFFERSIZE,0))>0){
        TRACE("message received is "<<buf_temp);
        TRACE("size recieved is "<<size_recv)
        buf_data.append(buf_temp,size_recv);
        
        unsigned long i = 0;
        if((i = buf_data.find("\r\n\r\n"))!=string::npos){
            break;
        }
    }
    
    if(size_recv<0){
		cerr<<"ERROR on reading data"<<endl;
		exit(1);
	}

	HttpRequest req;
	const char *buf3 = "GET http://www.google.com:80/ HTTP/1.1\r\n\r\n";
	req.ParseRequest(buf3, BUFFERSIZE);
	size_t size_req = req.GetTotalLength();
      
      char* ip = get_ip((req.GetHost()).c_str());
    
    char buf_req[size_req];
      bzero(buf_req, size_req);
      req.FormatRequest(buf_req);
     
      TRACE("ip is "<<ip<<endl)
      TRACE("buff is "<<buf_req);
      

      //====fetch data from remote server===
    TRACE("Now fetching data from the remote server");
      int sock_fetch = create_tcp_socket();
      struct sockaddr_in client;
      client.sin_family = AF_INET;
      client.sin_addr.s_addr = inet_addr(ip);
      client.sin_port = htons(req.GetPort());
      if(connect(sock_fetch, (struct sockaddr*)&client, sizeof(client))<0){
      	cerr<<"connect error when fetching data from remote server"<<endl;
      	exit(1);
      }
    
      TRACE("Connection established")
      if(send(sock_fetch, buf_req, size_req, 0)<0){
      	cerr<<"send failed when fetching data from remote server"<<endl;
	exit(1);
      }
      TRACE("Message sent to the remote server")
    buf_data.clear();
      bzero(buf_temp, BUFFERSIZE);
      ssize_t recv_size = 0;
      while((recv_size = recv(sock_fetch,buf_temp,BUFFERSIZE,0))>0){
          buf_data.append(buf_temp,recv_size);
	 
          if(buf_data.find("\r\n\r\n")!=string::npos){
	      TRACE(buf_data)
              break;
          }
	  //TRACE(buf_data);
      }
      if(recv_size < 0){
          cerr<<"ERROR on reading data"<<endl;
          exit(1);
      }
      close(sock_fetch);
    TRACE("Data received, forwarding to the client")
    data = buf_data.c_str();
    HttpResponse response;
    response.ParseResponse(data, sizeof(data));
    char buf_resp[response.GetTotalLength()];
    response.FormatResponse(buf_resp);
    
    send(temp_sock_desc, buf_resp, BUFFERSIZE, 0);

      TRACE("Done")
  //}
  close(temp_sock_desc);
  close(sock_desc);
  return 0;
}


