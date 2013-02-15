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
#include <time.h>
#include <map>
#include <sstream>

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

class HttpException : public std::exception
{
public:
    HttpException (const std::string &code, const std::string &reason) : m_reason (code+"/"+reason) { }
    virtual ~HttpException () throw () { }
    virtual const char* what() const throw ()
    { return m_reason.c_str (); }
private:
    std::string m_reason;
};


/**
 * @brief convert hostname to ip address
 * @TODO if cannot resolve host, what will happen?
 *
 * @param hostname
 * @return ip address
 */
char * get_ip(const char * host){
	struct hostent *hent;
	int iplen = 15;
	char *ip = (char*) malloc(iplen+1);
	memset(ip,0,iplen+1);
	if((hent = gethostbyname(host))==NULL){
		throw HttpException("404","Not found");
	}
	if(inet_ntop(AF_INET, (void *)hent->h_addr_list[0],ip,iplen)==NULL){
		throw HttpException("503","Bad Gateway");
	}
	return ip;
}

time_t convertTime(string ts){
    const char* format = "%a, %d %b %Y %H:%M:%S %Z";
    struct tm tm;
    if(strptime(ts.c_str(), format, &tm)==NULL){
    	return 0;
    }
    else{
    	return mktime(&tm);
    }
}

class Webpage{
public:
    Webpage(time_t expire, string lModify, string eTag, string dt){
        expireTime = expire;
        ETag = eTag;
        lastModify = lModify;
        data = dt;
    }
    
    time_t getExpire(void){
        return expireTime;
    }
    
    string getData(void){
        return data;
    }
    
    bool isExpired(){
        time_t now = time(NULL);
        if(difftime(expireTime, now)>0){
            return true;
        }
        else{
            return false;
        }
    }
    
    string getETag(){
        return ETag;
    }
    
    string getLastModify(){
        return lastModify;
    }
    
private:time_t expireTime;
    string lastModify;
    string ETag;
    string data;
};


class Cache{
public:
    Webpage* get(string url){
        map<string,Webpage>::iterator it;
        it = storage.find(url);
        if(it == storage.end()){
            return NULL;
        }
        else{
            return &it->second;
        }
    }
    
    void add(string url, Webpage pg){
        //storage[url]= pg;
	storage.erase(url);
	storage.insert(map<string, Webpage>::value_type(url, pg));
    }
    
    void remove(string url){
        storage.erase(url);
    }
    
private:
    map<string, Webpage> storage;
} cache;


/**
 * @brief get all the data for the request sent, and form the respone. 
 *      Assume that all package will have either content length or chunk
 * @TODO for package with status code other than 200, is the assumption true?
 *
 * @param the socket that the request has been sent
 * @return response data, and the response header 
 */
string fetchResponseData(int sckt, HttpResponse* response){
    bool isHeader = true;
    bool isChunk = false;
    
    unsigned long headerHead = string::npos;
    unsigned long headerTail = string::npos;
    
    ssize_t recv_size;
    string buf_data = "";
    char buf_temp[BUFFERSIZE];
    
    long contentLeft = 0;
    while((recv_size = recv(sckt,buf_temp,BUFFERSIZE,0))>0){
        buf_data.append(buf_temp,recv_size);
        string body;
        if(isHeader){
            //header hasn't been parsed yet
            if (headerHead == string::npos){
                headerHead= buf_data.find("HTTP/");
                if(headerHead == string::npos){
                    throw ParseException ("Incorrectly formatted response");
                }
            }
            if((headerTail = buf_data.find("\r\n\r\n",headerHead))!=string::npos){
                //finish receiving the header part
                string header = buf_data.substr(headerHead,headerTail+4 - headerHead);
                TRACE("header is:\n"<<header);
                body = buf_data.substr(headerTail+sizeof("\r\n\r\n")-1);
                TRACE("body is:\n"<<body<<"\n\n");
                
                TRACE("the size of header.c_str is "<<strlen(header.c_str())<<" and the size of header.length is "<<header.length()<<" and end-start is "<<(headerTail - headerHead));
                response->ParseResponse(header.c_str(), header.length());
                
                string contentLength = response->FindHeader("Content-Length");
                TRACE("Content Length is <"<<contentLength<<">")
                if(contentLength!=""){
                    contentLeft = atol(contentLength.c_str());
                    isChunk = false;
                    contentLeft -= body.size();
                }
                else{
                    TRACE("Find Transfer-Encoding "<<response->FindHeader("Transfer-Encoding"))
                    if(response->FindHeader("Transfer-Encoding")=="chunked"){
                        TRACE("chunked")
                        isChunk = true;
                        if(body.find("0\r\n\r\n")!=string::npos){
                            TRACE(body.substr(body.find("0\r\n\r\n")))
                            break;
                        }
                    }
                    else{
                        throw ParseException ("Incorrectly formatted response");
                    }
                }
                TRACE("isChunk: "<<isChunk);
                isHeader = false;
            }//Finish receiving all data for header
            else{
                isHeader = true;
            }
        }
        else{
            //check whether the message body has ended
          	if(isChunk){
                body = buf_temp;
                if(body.find("0\r\n\r\n")!=string::npos){
	      	  		TRACE(body.substr(body.find("0\r\n\r\n")))
                    break;
                }
          	}
          	else{
                	contentLeft -= recv_size;
	      		//TRACE("content left is "<<contentLeft)
			//TRACE("buf data is "<<buf_data)
                	if(contentLeft <=0){
                   		 break;
                	}
            }//if end is determined by content-length
        }//not header part
    }//while for reading data
    if(recv_size < 0){
        cerr<<"ERROR on reading data"<<endl;
        exit(1);
    }
    return buf_data;
}

long parseCacheControl(string s){
    if(s.find("private")!=string::npos || s.find("no-cache")!=string::npos || s.find("no-store")!=string::npos){
        return 0;
    }
    size_t pos = string::npos;
    if ((pos = s.find("max-age"))!=string::npos) {
        size_t numStart = s.find('=');
        if(numStart != string::npos){
            long age = atol(s.substr(numStart+1).c_str());
            //TRACE("max age is "<<time);
            return age;
        }
    }
    return 0;
}

string fetchResponse(HttpRequest req){
    ostringstream ss;
    ss<< req.GetHost()<<":"<<req.GetPort()<<req.GetPath();
    string url = ss.str();
    TRACE("url is "<<url);
    
    const char* host = (req.GetHost()).c_str();
    unsigned short port = htons(req.GetPort());
    char* ip  = get_ip(host);

    
    int sock_fetch = create_tcp_socket();
    struct sockaddr_in client;
    client.sin_family = AF_INET;
    client.sin_addr.s_addr = inet_addr(ip);
    client.sin_port = port;
    if(connect(sock_fetch, (struct sockaddr*)&client, sizeof(client))<0){
      	throw HttpException("502", "Bad Gateway");
    }
    
    
    TRACE("Connection established")
    size_t size_req = req.GetTotalLength();
    char buf_req[size_req];
    bzero(buf_req, size_req);
    req.FormatRequest(buf_req);
    if(send(sock_fetch, buf_req, size_req, 0)<0){
      	cerr<<"send failed when fetching data from remote server"<<endl;
        throw HttpException("502", "Bad Gateway");
    }
    TRACE("Message sent to the remote server")
    HttpResponse resp;
    string data = fetchResponseData(sock_fetch, &resp);
    TRACE("data is "<<data)
    string statusCode = resp.GetStatusCode();
    TRACE("status code is "<<statusCode);
    if (statusCode=="200") {
        //Normal data package
        string expire = resp.FindHeader("Expires");
        string ETag = resp.FindHeader("ETag");
        string date = resp.FindHeader("Date");
        string lastModi = resp.FindHeader("Lat-Modified");
        string cacheControl = resp.FindHeader("Cache-Control");
        TRACE("expire as "<<expire<<"\nETag as "<<ETag<<"\ndate as "<<date<<"\nlastModi as "<<lastModi
              <<"\ncacheControl as "<<cacheControl)
	time_t expire_t;
        if (expire != "" &&(expire_t = convertTime(expire))!=0) {
            TRACE("add to cache with nomarl expire");
            Webpage pg(expire_t, lastModi, ETag, data);
            cache.add(url, pg);
        }
        else if(cacheControl!=""){
            long maxAge = 0;
            if ((maxAge = parseCacheControl(cacheControl))!=0 && date!="") {
                time_t  expire_t = convertTime(date) + maxAge;
                TRACE("add to cache with cache control");
                Webpage pg(expire_t, lastModi, ETag, data);
                cache.add(url, pg);
            }
            else{   //cache not enable
                TRACE("cache control deny");
                cache.remove(url);
            }
        }
        else{ //cache not enable
            TRACE("no info for cache");
            cache.remove(url);
        }
    }
    else if(statusCode == "304"){ //content not changed
        data = cache.get(url)->getData();
        TRACE(304)
    }
    close(sock_fetch);
    return data;
}

string getResponse(HttpRequest req){
    ostringstream ss;
    ss<< req.GetHost()<<":"<<req.GetPort()<<req.GetPath();
    string url = ss.str();
    TRACE("url is "<<url);
    
    Webpage* pg = cache.get(url);
    if(pg!=NULL){
      TRACE("webpage in cache we get is "<<pg->getExpire());
      if (!pg->isExpired()){
          TRACE("webpage in cahce, not expired")
          return pg->getData();
      }
      else{
          if (pg->getETag() != "") {
              TRACE("try to use ETag")
              req.AddHeader("If-Non-Match", pg->getETag());
          }
          else if(pg->getLastModify() !=""){
              TRACE("try to use last modify")
              req.AddHeader("If-Modified-Since", pg->getLastModify());
          }
          return fetchResponse(req);
      }
    }
    else{
        TRACE("directly fetch");
        return fetchResponse(req);
    }
}

HttpResponse createErrorMsg(string reason){
    HttpResponse resp;
    resp.SetVersion("1.1");
    size_t split = reason.find('/');
    string code = reason.substr(0,split);
    string msg = reason.substr(split+1);
    resp.SetStatusCode(code);
    resp.SetStatusMsg(msg);
    return resp;
}



int main (int argc, char *argv[])
{
  // command line parsing
  
    
  socklen_t len;
  int sock_listen = create_tcp_socket();
  struct sockaddr_in serv_addr; 
  serv_addr.sin_family = AF_INET;
  serv_addr.sin_addr.s_addr = INADDR_ANY;//inet_addr("127.0.0.1");
  serv_addr.sin_port = htons(PORT);
  if((bind(sock_listen, (struct sockaddr*)&serv_addr, sizeof(serv_addr)))<0){
 	 cerr<<"ERROR on binding"<<endl;
	 exit(1);
  }
  TRACE("bind successful")
  if(listen(sock_listen,20)<0){
  	cerr<<"ERROR on listening"<<endl;
	exit(1);
  }
  TRACE("listen succesful")
  struct sockaddr_in cli_addr;
  len = sizeof(cli_addr);
  int sock_request = accept(sock_listen, (struct sockaddr*)&cli_addr, &len);
  if(sock_request < 0){
  	cerr<<"ERROR on accept"<<endl;
	exit(1);
  }
  TRACE("accept successful");
  while(1){
        string buf_data;
        char buf_temp[BUFFERSIZE];
        try{
            //get request from established connection
            ssize_t size_recv;
            while((size_recv = recv(sock_request,buf_temp,BUFFERSIZE,0))>0){
                TRACE("message received is "<<buf_temp)
                TRACE("size recieved is "<<size_recv)
                
                buf_data.append(buf_temp,size_recv);
                
                if((buf_data.find("\r\n\r\n"))!=string::npos){
                    break;
                }
            }
            if(size_recv == 0){ //the other side close the connection
                break;
            }
            //@TODO if size received is no bigger than 0, then just ignore this receive
            if(size_recv<0){
                throw ParseException("400/Bad Request");
            }
                            
            const char *buf3 = "GET http://www.zhiyangwang.org:80/ HTTP/1.1\r\n\r\n";
            
            HttpRequest req;
            req.ParseRequest(buf3, BUFFERSIZE);
            
          //====fetch data from remote server===
            TRACE("Now fetching data from the remote server");
            const char * data;          
          
          
          //@TODO if response not well formatted, then resend, if 3 time failure, then report to requester, this part should be surrounded by try catch
              try{
                  string d = getResponse(req);
              
                    TRACE("main get response")
                    data = d.c_str();
                    TRACE("data is "<<data)
                    TRACE("Data received, forwarding to the client")
                    send(sock_request, data, strlen(data) , 0);
                    //TRACE("size is "<<sizeof(data))
              
              }
              catch(ParseException ex){
                  TRACE("Parse Exception for response")
                  string reason = "502/BadGateway";
                  HttpResponse resp = createErrorMsg(reason);
                  char respD[resp.GetTotalLength()];
                  resp.FormatResponse(respD);
                  send(sock_request, respD, strlen(respD), 0);
              }
              catch(HttpException ex){
                  HttpResponse resp = createErrorMsg(ex.what());
                  char respD[resp.GetTotalLength()];
                  resp.FormatResponse(respD);
                  send(sock_request, respD, strlen(respD), 0);
              }
              TRACE("Done")
      }
      catch(ParseException ex){ //Request invalid
          TRACE("Parse Exception for request")
          HttpResponse resp = createErrorMsg(ex.what());
          char respD[resp.GetTotalLength()];
          resp.FormatResponse(respD);
          send(sock_request, respD, strlen(respD), 0);

      }
      catch(exception ex){
          TRACE("unexcepted exception "<<ex.what())
          break;
      }
  }
  close(sock_request);
  close(sock_listen);
  return 0;
}


