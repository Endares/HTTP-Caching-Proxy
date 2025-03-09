#include "proxy.h"
#include <thread>


#define CACHE_SIZE 10
#define MULTI_THREAD 1
#define STRING_502 "HTTP/1.1 502 Bad Gateway\r\nContent-Length: 0\r\n\r\n"
#define MSG_502 "HTTP/1.1 502 Bad Gateway"

namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;


mutex mutex_lock_;

void proxy::setup_listening_socket()
{
    struct sockaddr_in server_addr, player_addr;
    socklen_t addr_len = sizeof(player_addr);
    // create listening socket
    listening_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listening_fd == -1)
    {
        perror("Server socket creation failed");
        exit(EXIT_FAILURE);
    }
    // set port reused
    int optval = 1;
    if (setsockopt(listening_fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
    {
        std::cerr << "Failed to set SO_REUSEADDR!" << std::endl;
        close(listening_fd);
        exit(EXIT_FAILURE);
    }
    // bind port
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(this->server_port); // host order to network order
    if (::bind(listening_fd, (struct sockaddr *)(&server_addr), sizeof(server_addr)) < 0)
    {
        perror("Server socket bind port failuer");
        close(listening_fd);
        exit(EXIT_FAILURE);
    }
    // start listening for connection
    if (listen(listening_fd, MAX_CLIENT) < 0)
    {
        // if listen failed
        perror("Listen failed");
        close(listening_fd);
        exit(EXIT_FAILURE);
    }
}
proxy::proxy(int listening_port, size_t cache_capacity, const string &log_path) : server_port(listening_port), cache(cache_capacity), logger(log_path)
{
    // set logger

    // set status of socket as listening
    setup_listening_socket();
    // wait for client connection
    accept_connection();
}

/**
 *  wait for client connection
 */
void proxy::accept_connection()
{
    sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    while (true)
    {
        int client_fd = accept(listening_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0)
        {
            perror("client connect failed");
            close(client_fd);
            exit(EXIT_FAILURE);
        }
        // parse the ip and port
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        uint16_t client_port = ntohs(client_addr.sin_port);
        request_uid++; // update request uid
        client_connection_info request_client(client_ip, client_fd, client_port, request_uid);
        if (MULTI_THREAD)
        {
            try
            {
                // std::thread handle_request_thread([this, request_client]()
                //                                   { this->handle_request(request_client); });
                // handle_request_thread.detach();
                thread_pool.AddJob([this,request_client](){handle_request(request_client);});
            }
            // create a thread to handle request
            catch (runtime_error &e)
            {
                cerr << e.what() << endl;
            }
        }
        else
        {
            handle_request(request_client);
        }
    }
}
void proxy::log_new_http_request(const http::request<http::string_body> &request, const client_connection_info &client_info)
{

    std::string time_received = get_current_time();

    // extract http request line
    std::string request_line = std::string(request.method_string()) + " " +
                               std::string(request.target());

    // extract client IP
    string IP = client_info.get_client_host();
    string log_entry = to_string(request_uid) + ": " + request_line + " from " + IP + " @ " + time_received;
    logger.log(log_entry);
}
int proxy::handle_request(const client_connection_info &connection_info)
{
    vector<char> request_buffer(BUFFER_SIZE, 0);
    int status = recv(connection_info.get_to_client_fd(), request_buffer.data(), BUFFER_SIZE, 0);
    if (status <= 0)
    {
        // close connection
        close(connection_info.get_to_client_fd());
        return 0;
    }
    
    string request_str(request_buffer.begin(), request_buffer.begin() + status);
    // cout << status << endl;
    // string request_str;
    // int status = recv_http_request(connection_info.get_to_client_fd(),request_str);
    //  receive correctly
    //  parse http request string to boost::http_request
    http::request<http::string_body> req;
    try
    {
        req = parse_http_req(request_str);
    }
    catch (const runtime_error &e)
    {
        // string error_502 = "HTTP/1.1 502 Bad Gateway\r\n0\r\n";
        auto error_502 = STRING_502;
        auto http_502 = parse_http_response(error_502);
        send_response_back_to_client(connection_info.get_to_client_fd(),connection_info.get_request_id(),error_502, MSG_502);
        return -1;
    }

    // std::cout << "Method: " << req.method_string() << "\n";              // GET
    // std::cout << "Target: " << req.target() << "\n";                     // /index.html
    // std::cout << "Version: " << req.version() << "\n";                   // 11 (表示 HTTP/1.1)
    // std::cout << "Host: " << req[http::field::host] << "\n";             // www.example.com
    // std::cout << "User-Agent: " << req[http::field::user_agent] << "\n"; // Mozilla/5.0
    // std::cout << "Connection: " << req[http::field::connection] << "\n"; // keep-alive
    log_new_http_request(req, connection_info);
    try
    {
        if (req.method_string() == "GET")
        {
            handle_get(req, connection_info);
        }
        else if (req.method_string() == "POST")
        {
            handle_post(req, connection_info);
        }
        else if (req.method_string() == "CONNECT")
        {
            handle_connect(req, connection_info);
        } else {

        }
    }
    catch(runtime_error &e){
        logger.log(to_string(connection_info.get_request_id()) + " Error " + e.what());
    }

    logger.log(to_string(connection_info.get_request_id()) + ": Tunnel closed");
    close(connection_info.get_to_client_fd());
    // cout << "handle ending" << endl;
    return status;
}
// return a server fd to server
int proxy::connect_to_send_request(const http::request<http::string_body> &req, bool if_https)
{
    std::string host = string(req[boost::beast::http::field::host]); // this field have had port
    std::string port = if_https ? "443" : "80";              // default port 80 for http request, 443 for https connect method
    size_t colon_pos = host.find(':');
    if (colon_pos != std::string::npos)
    {
        port = host.substr(colon_pos + 1); // analyze target port
        host = host.substr(0, colon_pos);  // analyze target ip
    }
    int to_real_server_fd;
    try { // invalid server hostname / create server fd failed
        to_real_server_fd = connect_to_server(host, atoi(port.c_str()));
    } catch(const runtime_error &e){
        throw std::runtime_error(std::string("invalid socket to server: ") + e.what());
    }
    // send_http_request(to_real_server_fd,req);
    return to_real_server_fd;
}
void proxy::send_response_back_to_client(int which_fd,int req_id, const http::response<http::string_body> &res){
    // std::string http_version = "HTTP/" + std::to_string(resp.version() / 10) + "." + std::to_string(resp.version() % 10);
    // std::string status_code = std::to_string(static_cast<int>(resp.result()));
    // string status_code = "1.1";
    std::string http_version = "HTTP/" + std::to_string(res.version() / 10) + "." + std::to_string(res.version() % 10);
    std::string status_code = std::to_string(static_cast<int>(res.result()));
    std::string reason = string(res.reason());

    // merge http response lines
    std::string response_line = http_version + " " + status_code + " " + reason;
    int status = send_http_response(which_fd,res);
    if(status != 0){
        logger.log(to_string(req_id) + " : Error: send HTTP response failed");
        throw runtime_error("send response back to client error");
    }
    logger.log(to_string(req_id) + ": Responding " + response_line);
}
// overloading
void proxy::send_response_back_to_client(int which_fd,int req_id, const char* response_str, const char* response_msg){
    int status = send_http_response(which_fd,response_str);
    if(status != 0){
        throw runtime_error("send response back to client error");
    }
    logger.log(to_string(req_id) + ": Responding " + response_msg);
}
void proxy::handle_get(http::request<http::string_body> &req, const client_connection_info &client_info)
{
    // parse request and get url
    string url;

    try
    {
        url = getFullUrl(req);
    }
    catch (runtime_error &e)
    {
        // log the error
        return;
    }
    modify_request(req); // modify connection strategy of request to close
    // cout << url << endl;
    // to be done: client can send request that "I don't want get it from cache",
    // we need to parse the request and find it
    // find from cache
    shared_ptr<http::response<http::string_body>> response_ptr = nullptr;
    bool needRevalidation = client_need_revalidation(req);                      // find if client's request says we need revalidation
    int cache_status = cache.try_get_data(url, needRevalidation, response_ptr); // try to get it from cache, with
    // different strategy
    //  if exist and not expired and don't need revalidation, send back to client
    if (cache_status == CAN_RETURN_DIRECTLY)
    {
        logger.log(to_string(request_uid) + ": in cache, valid");
        // send_http_response(client_info.get_to_client_fd(), *(response_ptr.get()));
        send_response_back_to_client(client_info.get_to_client_fd(),client_info.get_request_id(),*(response_ptr.get()));
        return;
    }
    int to_real_server_fd = 0;
    try{
        to_real_server_fd =  connect_to_send_request(req, false);
        //connection established fail
        //send error back to client
    }
    catch(const runtime_error &e){
        // string error_502 = "HTTP/1.1 502 Bad Gateway\r\n0\r\n";
        auto error_502 = STRING_502;
        auto http_502 = parse_http_response(error_502);
        send_response_back_to_client(client_info.get_to_client_fd(),client_info.get_request_id(),error_502,MSG_502);
        return;
    }
    
    // connection establish success
    if (cache_status == NOT_EXIST)
    {
        // cache entry doesn't exist, fetch it from server
        logger.log(to_string(request_uid) + ": not in cache");
        int send_status = send_http_request(to_real_server_fd, req);
        if (send_status != 0)
        {
            cerr << "send request to server error" << endl;
            // return;
        }
        // get response from server
        string response_str;
        int recv_status = recv_http_response(to_real_server_fd, response_str);
        if (recv_status < 0)
        {
            cerr << "receive response from server error in get" << endl;
            // return;
        }
        // add it to my cache
        http::response<http::string_body> myresponse = parse_http_response(response_str);
        cache.put(url, make_shared<http::response<http::string_body>>(myresponse));
        // send back to client
        send_response_back_to_client(client_info.get_to_client_fd(),client_info.get_request_id(),myresponse);
        close(to_real_server_fd);
    }
    else if (cache_status == NEED_REVALIDATION)
    {
        logger.log(to_string(client_info.get_request_id()) + ": requires validation");
        // compose revalidate request
        auto revalidate_request = compose_revalidate_request(*(response_ptr.get()), req);
        // danger: what if response in cache doesn't have enough information?(no etag or no last-modified)

        // send revalidate request to server
        send_http_request(to_real_server_fd, revalidate_request);
        // get response from server, if it's 304, return the response in cache
        //(multi-thread danger:what if we don't have it now in our cache?)
        auto revalidate_response = recv_revalidate_response(to_real_server_fd);
        // if it's 200 and we don't have private cache-control in our request, update cache
        auto response_status = revalidate_response.result();
        // cout << response_status << endl;
        if (response_status == http::status::ok)
        {
            // we can't directly put it into cache, verify it first
            cache_response(url, revalidate_response, client_info.get_request_id());
            send_response_back_to_client(client_info.get_to_client_fd(),client_info.get_request_id() ,revalidate_response);
            close(to_real_server_fd);
            return;
        }
        else if (response_status == http::status::not_modified)
        {
            
            // logger.log(to_string(client_info.get_request_id()) + )
            send_response_back_to_client(client_info.get_to_client_fd(),client_info.get_request_id(),*(response_ptr.get()));
            // need to modify expire date in cache
            cache.put(url, make_shared<http::response<http::string_body>>(revalidate_response)); // also need to update cache
            // because we need to update modified-since and maxage
            close(to_real_server_fd);
            return;
        }
        // send back to client
        else
        {
            // error status
            throw runtime_error("Revalidation got invalid status");
            close(to_real_server_fd);
            return;
        }
    }
}
void proxy::cache_response(const string &url, const http::response<http::string_body> &resp, int req_id)
{
    auto cache_control_it = resp.find(http::field::cache_control);
    if (cache_control_it == resp.end())
    {
        // no cache-control field, can cache
        //  logger.log(to_string(to_string(req_id) + ": " ));
    }
    string cache_control_info = string(cache_control_it->value());
    if (cache_control_info.find("max-age=0") != string::npos)
    {
        logger.log(to_string(req_id) + ": not cacheble because response has max-age = 0");
        return;
    }
    if (cache_control_info.find("no-store") != string::npos)
    {
        logger.log(to_string(req_id) + ": not cacheble because response is no-store");
        return;
    }
    if (cache_control_info.find("no-cache") != string::npos)
    {
        logger.log(to_string(req_id) + ": not cacheble because response is no-cache");
        return;
    }

    if (cache_control_info.find("private") != string::npos)
    {
        logger.log(to_string(req_id) + ": not cacheble because response is private");
        return;
    }

    // can cache
    cache.put(url, make_shared<http::response<http::string_body>>(resp));
    if (cache_control_info.find("max-age="))
    {
        logger.log(to_string(req_id) + ": cached, but requires re-validation");
        return;
    }
    // can cache
    auto expires_ptr = resp.find(http::field::expires);
    if (expires_ptr != resp.end())
    {
        // if expired is assigned
        std::string expires_time = string(expires_ptr->value());
        logger.log(to_string(req_id) + ": cached, expires at " + expires_time);
    }
}
void proxy::handle_post(http::request<http::string_body> &req, const client_connection_info &client_info)
{

    // request from server
    int to_real_server_fd = 0;
    try{
        to_real_server_fd =  connect_to_send_request(req, false);
        //connection established fail
        //send error back to client
    }
    catch(const runtime_error &e){
        // string error_502 = "HTTP/1.1 502 Bad Gateway\r\n0\r\n";
        auto error_502 = STRING_502;
        auto http_502 = parse_http_response(error_502);
        logger.log(to_string(client_info.get_request_id()) + " : Error: Failed to connect to server");
        send_response_back_to_client(client_info.get_to_client_fd(),client_info.get_request_id(),error_502,MSG_502);
        return;
    }
    
    modify_request(req);
    if (send_http_request(to_real_server_fd, req) == -1) {
        logger.log(to_string(client_info.get_request_id()) + " : Error: send HTTP request failed");
    }
    // get the response
    string response_str;

    // status is the length of http response
    int status = recv_http_response(to_real_server_fd, response_str);
    // cout << "response length " << response_str.length() << endl;
    // cout << response_str << endl;
    // close(to_real_server_fd);

    // send response back to client
    auto response = parse_http_response(response_str);
    send_response_back_to_client(client_info.get_to_client_fd(),client_info.get_request_id(),response);
    // close(client_info.get_to_client_fd());
    close(to_real_server_fd);
}

void proxy::handle_connect(http::request<http::string_body> &req, const client_connection_info &client_info)
{
    // modify request
    modify_request(req);
    // request from server
    int to_real_server_fd = 0;
    try{
        to_real_server_fd =  connect_to_send_request(req, false);
        //connection established fail
        //send error back to client
    }
    catch(const runtime_error &e){
        // string error_502 = "HTTP/1.1 502 Bad Gateway\r\n0\r\n";
        auto error_502 = STRING_502;
        auto http_502 = parse_http_response(error_502);
        send_response_back_to_client(client_info.get_to_client_fd(),client_info.get_request_id(),error_502,MSG_502);
        return;
    }
    
    // send msg to client: 200 OK
    int to_client_real_fd = client_info.get_to_client_fd();
    string ok_msg = "HTTP/1.1 200 Connection Established\r\n\r\n";
    send(to_client_real_fd, ok_msg.data(), ok_msg.length(), 0);
    fd_set fds;
    int client_fd = client_info.get_to_client_fd();
    int nfds = max(client_fd, to_real_server_fd);

    bool flag = true;
    while (flag)
    {
        if (!flag)
        {
            break;
        }
        FD_ZERO(&fds);
        FD_SET(to_real_server_fd, &fds);
        FD_SET(client_fd, &fds);
        int status = select(nfds + 1, &fds, NULL, NULL, NULL);
        if (status == 0)
        {
            break;
        }
        else if (status == -1)
        {
            break;
        }
        // In CONNECT mode, the client controls the tunnel's lifetime.
        // We must check client_fd first so that if the client disconnects (recv <= 0),
        // we immediately exit the loop. Checking the server first may miss the client's disconnect,
        // keeping the thread occupied.
        if (FD_ISSET(client_fd, &fds)) {
            if (forward_data_once(client_fd, to_real_server_fd) == -1)
            {
                return;
            }
        }
        if (FD_ISSET(to_real_server_fd, &fds)) {
            if (forward_data_once(to_real_server_fd, client_fd) == -1)
            {
                return;
            }
        }
    }
    //close(to_real_server_fd); Shouldn't close here!
    // logger.log(to_string(client_info.get_request_id()) + " connectiont return");
}
http::request<http::string_body> proxy::compose_revalidate_request(const http::response<http::string_body> &old_response,
                                                                   const http::request<http::string_body> &old_request)
{
    // parse old response, get the etag and last-modified
    auto etag_it = old_response.find(http::field::etag);
    auto last_modified_it = old_response.find(http::field::last_modified);
    http::request<http::string_body> revalidate_request = old_request;
    if (etag_it != old_response.end())
    {
        // append etag to if-non-match field of request
        revalidate_request.set(http::field::if_none_match, etag_it->value());
    }
    if (last_modified_it != old_response.end())
    {
        // append if-modified-since
        revalidate_request.set(http::field::if_modified_since, last_modified_it->value());
    }
    return revalidate_request;
}
http::response<http::string_body> proxy::recv_revalidate_response(int which_fd)
{
    string response_str;
    int recv_status = recv_http_response(which_fd, response_str);
    if (recv_status <= 0)
    {
        throw runtime_error("receive revalidate response from server failed");
    }
    return parse_http_response(response_str);
}
client_connection_info::client_connection_info(string client_host, int whichfd, int client_port, int request_id) : client_host(client_host), to_client_fd(whichfd), client_side_port(client_port), request_id(request_id)
{
}

int main()
{
    proxy p(12345, CACHE_SIZE, "/var/log/erss/proxy.log");
    return 0;
}