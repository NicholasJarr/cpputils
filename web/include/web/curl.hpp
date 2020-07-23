#ifndef WEB_CURL_HPP
#define WEB_CURL_HPP

#include <functional>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#include <curl/curl.h>

namespace web
{
    using byte_buffer = std::vector<uint8_t>;

    using curl_ptr = std::unique_ptr<CURL, std::function<void(CURL *)>>;
    using curl_slist_ptr = std::unique_ptr<curl_slist, std::function<void(curl_slist *)>>;

    class curl_global_handle
    {
    public:
        curl_global_handle();
        ~curl_global_handle();
    };

    class curl
    {
    public:
        class curl_error : public std::runtime_error
        {
        public:
            curl_error(const std::string &what);
        };

        curl();
        virtual ~curl();

        std::string get(const std::string &url, bool should_reset = true);
        std::string post(const std::string &url, bool should_reset = true);
        std::string post(const std::string &url, const std::string &body, bool should_reset = true);
        std::string post_text(const std::string &url, const std::string &text);
        std::string post_json(const std::string &url, const std::string &json_string);
        std::string url_encode(const std::string &url);

    protected:
        curl_ptr curl_wrapper_;
        std::stringstream response_, request_;
        int request_size_;

        static size_t write_response(void *buffer, size_t size, size_t nmemb, void *userp);
        static size_t write_request(void *buffer, size_t size, size_t nmemb, void *userp);
    };
} // namespace web

#endif