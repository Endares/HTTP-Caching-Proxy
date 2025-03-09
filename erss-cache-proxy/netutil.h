#include <iostream>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include <netdb.h>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#define BUFFER_SIZE 10e5
#define CAN_CACHE 0
#define CANNOT_CACHE 1
using namespace std;
namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
#ifndef NET_UTIL
#define NET_UTIL
int connect_to_server(string server_ip, int server_port)
{
    struct sockaddr_in server_addr;
    // get ip by hostname
    struct hostent *host = gethostbyname(server_ip.c_str());
    if (host == nullptr)
    {
        // cerr << "Error: failed to resolve hostname" << endl;
        throw runtime_error("can't resolve hostname");
    }
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, host->h_addr_list[0], ip, sizeof(ip));
    // create socket
    int to_server_fd = socket(AF_INET, SOCK_STREAM, 0);

    if (to_server_fd < 0)
    {
        // cerr << "Error: failed to create socket" << endl;
        throw runtime_error("create socket failed");
        // exit(EXIT_FAILURE);
    }

    // connect to server
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    inet_pton(AF_INET, ip, &server_addr.sin_addr);

    if (::connect(to_server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        
        ::close(to_server_fd);
        throw runtime_error("connect to server error");
        // exit(EXIT_FAILURE);
    }
    // cout << "connect to server success" << endl;
    return to_server_fd;
}

http::request<http::string_body> parse_http_req(const string &request_data)
{
    // cout << request_data.size() << endl;
    //what if we receive an empty request_data?
    http::request_parser<http::string_body> parser;
    beast::error_code ec;
    parser.eager(true);
    // if normal, ec = 0; error: != 0
    parser.put(asio::buffer(request_data), ec);
    if (ec)
    {
        // cout << ec << endl;
        throw std::runtime_error("Failed to parse HTTP request: " + ec.message());
    }
    return parser.release();
}
http::response<http::string_body> parse_http_response(const string &responseStr)
{
    http::response_parser<http::string_body> parser;
    boost::beast::error_code ec;

    parser.eager(true); // parse body
    parser.put(boost::asio::buffer(responseStr), ec);

    if (ec || !parser.is_done()) { // parse failure
        // cout << responseStr << endl;
        throw std::runtime_error("Failed to parse HTTP response: " + ec.message());
    }

    return parser.release();
}
// overload
int send_http_request(int sockfd, const string &request)
{
    size_t total_sent = 0;
    size_t request_len = request.length();

    while (total_sent < request_len)
    {
        int sent_bytes = send(sockfd, request.data() + total_sent, request_len - total_sent, 0);

        if (sent_bytes < 0)
        {
            // std::cerr << "Error: send HTTP request failed" << std::endl;
            return -1; // send failed
        }

        total_sent += sent_bytes;
    }

    return 0; // send success
}
int send_http_request(int to_server_fd, const http::request<http::string_body> &req)
{
    std::ostringstream oss;
    oss << req;
    std::string req_str = oss.str();
    return send_http_request(to_server_fd, req_str);
}

/**
 * receive an http response from receive_fd
 * return value: status
 * get response from parameter "response"
 */
int recv_http_response(int receive_fd, string &response)
{
    vector<char> response_buffer(BUFFER_SIZE, 0);
    response.clear();
    // int total_len = 0;
    size_t check_pos = 0; // record last time check position
    while (true)
    {
        int status = recv(receive_fd, response_buffer.data(), BUFFER_SIZE, 0);

        if (status < 0)
        {
            // std::cerr << "Error: proxy receive failed" << std::endl;
            return -1;
        }
        else if (status == 0)
        {
            // std::cerr << "recv_http_request error: Client disconnected (fd: " << receive_fd << ")" << std::endl;
            break;
        }
        // cout << "data: " << response_buffer.data() << endl;
        // append received data
        response.append(response_buffer.begin(), response_buffer.begin() + status);
        // cout << "response: " << response << endl;
        // total_len += status;
        // check if receive
        if (response.find("\r\n0\r\n", check_pos) != string::npos)
        {
            // cout << "IS END!" << endl;
            break;
        }
        // optimize, check new chunk, instead of checking the whole string
        check_pos = response.size();
    }

    return response.size();
}

int recv_http_request(int receive_fd, string &request)
{
    // request.clear();
    vector<char> request_buffer(BUFFER_SIZE, 0);
    request.clear();
    size_t check_pos = 0;
    while (true)
    {
        int status = recv(receive_fd, request_buffer.data(), BUFFER_SIZE, 0);

        if (status < 0)
        {
            // std::cerr << "Error: proxy receive failed" << std::endl;
            return -1;
        }
        else if (status == 0) // that's a normal case, because we use while loop
        {
            return 0;
        }

        // append received message
        request.append(request_buffer.begin(), request_buffer.begin() + status);

        if (request.find("\r\n0\r\n", check_pos) != string::npos)
        {
            // cout << "IS END!" << endl;
            break;
        }
        check_pos = request.size();
    }

    return request.size();
}
/**
 * receive http request depends on different send strategy
 * return receive length
 * add request into param request
 */
// int recv_http_request_v2(int receive_fd, string &request){
//     //receive the header
//     //if header has content-length, receive the body of content length
//     //else receive untill end of http request \r\n0\r\n

// }

int send_http_response(int sockfd, const string &response)
{
    size_t total_sent = 0;
    size_t request_len = response.length();

    while (total_sent < request_len)
    {
        int sent_bytes = send(sockfd, response.data() + total_sent, request_len - total_sent, 0);

        if (sent_bytes < 0)
        {
            // std::cerr << "Error: send HTTP response failed" << std::endl;
            return -1;
        }

        total_sent += sent_bytes;
    }

    return 0;
}
// overload
int send_http_response(int to_server_fd, const http::response<http::string_body> &req)
{
    std::ostringstream oss;
    oss << req;
    std::string req_str = oss.str();
    return send_http_response(to_server_fd, req_str);
}
int forward_data_once(int from_fd, int to_fd)
{
    vector<char> buffer(65536, 0);
    int len_recv = recv(from_fd, buffer.data(), buffer.size(), 0);
    if (len_recv <= 0) // fd closed or error
    {
        return -1;
    }
    int len_send = send(to_fd, buffer.data(), len_recv, 0);
    if (len_send <= 0) // fd closed or error
    {
        return -1;
    }
    return 0; // normal return, fd not closed on the other side
}
// change connection type from keep-alive to close
void modify_response(http::response<http::string_body> &res)
{
    // if (res[http::field::connection] == "keep-alive")
    // if (res[http::field::connection] == "keep-alive") {
    res.set(http::field::connection, "close");
    // }
}
void modify_request(http::request<http::string_body> &res)
{
    // if (res[http::field::connection] == "keep-alive") {
    res.set(http::field::connection, "close");
    // }
}
std::string getFullUrl(const http::request<http::string_body> &req, const std::string &protocol = "http")
{
    // get host field
    auto host_it = req.find(http::field::host);
    if (host_it == req.end())
    {
        throw std::runtime_error("Host header is missing");
    }

    std::string host = string(host_it->value());

    // acquire target
    std::string target = req.target().to_string();

    // construct full URL
    std::string url = protocol + "://" + host + " " + target;
    return url;
}
// parse http request, find if client's request require cache to revalidate
bool client_need_revalidation(const http::request<http::string_body> &req)
{
    auto cache_control_it = req.find(http::field::cache_control);
    if (cache_control_it == req.end())
    {
        return false;
    }
    string cache_control_info = string(cache_control_it->value());
    if (cache_control_info.find("max-age=0") != string::npos || cache_control_info.find("no-store") != string::npos || cache_control_info.find("no-cache") != string::npos 
            || cache_control_info.find("must-revalidate") != string::npos
                    ||cache_control_info.find("private") != string::npos){
        return true;
    }
    return false;
}
std::string get_current_time()
{
    // Get the current time point
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);

    // Convert to a tm structure for formatting
    std::tm tm = *std::localtime(&now_time);

    // Format the time as a string
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buffer);
}

#endif
