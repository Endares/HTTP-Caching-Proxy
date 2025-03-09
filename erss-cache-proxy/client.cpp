#include "client.h"
#include "netutil.h"
client::client(string host_name,string proxy_ip,int proxy_port):proxy_listening_port(proxy_port),
hostname(host_name),proxy_ip(proxy_ip){
    // to_proxy_fd = socket(AF_INET, SOCK_STREAM, 0);
    connect_to_proxy();
}
void client::connect_to_proxy(){
    // struct sockaddr_in server_addr;

    //     // create socket
        
    //     if (to_proxy_fd < 0)
    //     {
    //         perror("Socket failed");
    //         exit(EXIT_FAILURE);
    //     }

    //     // connect to server
    //     server_addr.sin_family = AF_INET;
    //     server_addr.sin_port = htons(proxy_listening_port);
    //     inet_pton(AF_INET, proxy_ip.c_str(), &server_addr.sin_addr);

    //     if (connect(to_proxy_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    //     {
    //         perror("Connect failed");
    //         close(to_proxy_fd);
    //         exit(EXIT_FAILURE);
    //     }
    to_proxy_fd =  connect_to_server(proxy_ip,proxy_listening_port);
}
int main(){
    client c("127.0.0.1","127.0.0.1",12345);
    return 0;
}