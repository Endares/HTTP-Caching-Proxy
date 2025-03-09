#include<iostream>
#include<sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<unistd.h>
#include<string>
using namespace std;
class client{
private:
    int to_proxy_fd;
    int proxy_listening_port;
    string hostname;
    string proxy_ip;
    // int request_uid;
    void connect_to_proxy();
public:
    client(string host_name,string proxy_ip,int proxy_port);
    string get_hostname() const{
        return hostname;
    }
    int get_to_server_fd() const{
        return to_proxy_fd; 
    }
    int get_proxy_listening_port() const{
        return proxy_listening_port;
    }
};
