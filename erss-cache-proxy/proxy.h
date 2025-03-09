#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include"netutil.h"
#include "proxyCache.h"
#include "logger.h"
#include"ThreadPool.h"
using namespace std;
#define MAX_CLIENT 20
#define THREAD_NUM 40
#ifndef PROXY
#define PROXY
class client_connection_info
{
private:
    string client_host;
    int to_client_fd;
    int client_side_port;
    int request_id;
    
public:
    client_connection_info(string client_host,int whichfd,int client_port,int request_id);
    string get_client_host() const{
        return client_host;
    }
    int get_to_client_fd() const{
        return to_client_fd;
    }
    int get_client_side_port() const{
        return client_side_port;
    }
    int get_request_id() const{
        return request_id;
    }
};

class proxy
{
private:
    int listening_fd;
    int server_port;

    void setup_listening_socket();
    void accept_connection(); // each connection represents a http request, use "short-lived connection"
    proxyCache cache;
    int connect_to_send_request(const http::request<http::string_body> &req, bool is_https); 
    //get the target server by analyzing the request send by client, connect to real server
    void handle_get(http::request<http::string_body> &req,const client_connection_info &client_info);   //handle get request
    void handle_post(http::request<http::string_body> &req,const client_connection_info &client_info);  // handle post request
    void handle_connect(http::request<http::string_body> &req,const client_connection_info &client_info);  // handle connect request
    http::request<http::string_body> compose_revalidate_request(const http::response<http::string_body> &old_response, const http::request<http::string_body> &old_request);  //compose a revalidate request 
    http::response<http::string_body> recv_revalidate_response(int which_fd);
    void log_new_http_request(const http::request<http::string_body> &request,const client_connection_info &client_info);
    void cache_response(const string &url,const http::response<http::string_body> &resp,int req_id);
    void send_response_back_to_client(int which_fd,int req_id, const http::response<http::string_body> &resp);
    void send_response_back_to_client(int which_fd,int req_id, const char* response_str, const char* response_msg);
    std::atomic<int> request_uid{0};
    Logger logger;
    ThreadPool<THREAD_NUM> thread_pool;
public:
    proxy(int listening_port,size_t cache_capacity,const string &log_path);
    int handle_request(const client_connection_info &connection_info); // handle requests
    
};

#endif
