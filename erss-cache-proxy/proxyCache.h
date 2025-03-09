#include "lruCache.h"
#include <string>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/asio.hpp>
#include <iomanip>
#include <sstream>
#include <ctime>
#ifndef PROXY_CACHE
#define PROXY_CACHE
#define NOT_EXIST -1
#define NEED_REVALIDATION -2
#define CAN_RETURN_DIRECTLY 0
using namespace std;
namespace beast = boost::beast;
namespace http = beast::http;
namespace asio = boost::asio;
// key: url, value http response
class proxyCache : public LRUCache<string, shared_ptr<http::response<http::string_body>>>
{
public:
    proxyCache(size_t capacity) : LRUCache(capacity)
    {
    }

    int try_get_data(const string &url, bool need_revalidation, shared_ptr<http::response<http::string_body>> &res_req)
    {
        // std::lock_guard<std::mutex> lock(mutex_);
        if (!checkIfExist(url))
        {
            // resource doesn't exist in cache
            return NOT_EXIST;
        }
        res_req = get(url);
        if (need_revalidation)
        { // need revalidation because client require
            return NEED_REVALIDATION;
        }
        // if expire, return NULL
        if (checkIfExpire(url))
        { // need revalidation beacause of expiration
            return NEED_REVALIDATION;
        }

        // don't need revalidation, return directly
        return CAN_RETURN_DIRECTLY;
    }

private:
    /**
     * check if get request exist in cache
     */
    bool checkIfExist(const string &url)
    {
        return peek(url) != nullptr;
    }
    bool checkTimeExpired(const string &response_date, int max_age = 0)
    {
        std::tm tm = {};
        std::istringstream ss(response_date);
        ss >> std::get_time(&tm, "%a, %d %b %Y %H:%M:%S GMT");
        std::time_t response_time_t = timegm(&tm);
        auto response_time = std::chrono::system_clock::from_time_t(response_time_t);

        // check if expire
        auto expiration_time = response_time + std::chrono::seconds(max_age);
        auto now = std::chrono::system_clock::now(); // get current time
        if (now > expiration_time)
        {
            // cout << "expired using maxage" << endl;
            return true; // expired
        }
        else
        {
            // cout << "haven't expired using maxage" << endl;
            return false; // haven't expired
        }
    }
    /**
     * check if a cache entry is expired
     */
    bool checkIfExpire(const string &url)
    {
        if (!checkIfExist(url)) // response must exist
        {
            return true;
        }
        auto corresponding_response = peek(url); // find the corresponding http response pointer

        // check Cache-Control: max-age
        auto cache_control_it = corresponding_response->find(http::field::cache_control);
        if (cache_control_it != corresponding_response->end())
        {
            std::string cache_control = string((cache_control_it->value()));
            // if we have max-age
            size_t max_age_pos = cache_control.find("max-age=");
            // if we have max age field, we must ignore expire field
            if (max_age_pos != std::string::npos)
            {
                // extract value of max_age_pos
                size_t max_age_end = cache_control.find(',', max_age_pos);
                string max_age_str;
                if (max_age_end == string::npos)
                {
                    // can't find ',' behind max_age field, only one field exist, or max-age is the last field
                    max_age_str = cache_control.substr(max_age_pos + 8);
                }
                else
                {
                    // have other field, parse max_age from max_age_pos + 8 to max_age_end
                    max_age_str = cache_control.substr(max_age_pos + 8, max_age_end - (max_age_pos + 8));
                }

                int max_age = std::stoi(max_age_str);

                // get the time respond was created and sent by the server
                auto date_it = corresponding_response->find(http::field::date);
                if (date_it != corresponding_response->end())
                {
                    std::string date_str = string(date_it->value());
                    return checkTimeExpired(date_str,max_age);
                }
                else
                {
                    return true; // if date is null, it's a undefined error, we recognize as it's expired
                }
            }
        }

        // check expire
        auto expires_it = corresponding_response->find(http::field::expires);
        if (expires_it != corresponding_response->end())
        {
            std::string expires_str = string(expires_it->value());
            return checkTimeExpired(expires_str);
        }
        return false;
    }
};
#endif