#include <web/ftp.hpp>

using namespace web;

ftp_curl::ftp_curl(const std::string &url, const std::string &credentials) : url_(url), credentials_(credentials) {}

void ftp_curl::send_file(const std::string &file_name, const byte_buffer &buffer, bool should_reset)
{
    CURLcode res;
    size_t http_code;
    std::string request_url = url_ + "/" + file_name;

    this->request_.str(std::string(buffer.begin(), buffer.end()));
    this->request_size_ = buffer.size();

    if (should_reset)
        curl_easy_reset(this->curl_wrapper_.get());

    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_URL, request_url.c_str());
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_USERPWD, credentials_.c_str());
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_UPLOAD, 1L);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_READFUNCTION, curl::write_request);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_READDATA, this);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_INFILESIZE_LARGE, (curl_off_t)request_size_);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_LOW_SPEED_TIME, 60L);  // timeout period
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_LOW_SPEED_LIMIT, 30L); // number of bytes during timeout period

    // TODO: Remoev
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_VERBOSE, 1L); // number of bytes during timeout period

    res = curl_easy_perform(this->curl_wrapper_.get());
    if (res != CURLE_OK)
    {
        throw curl::curl_error(curl_easy_strerror(res));
    }
    curl_easy_getinfo(this->curl_wrapper_.get(), CURLINFO_RESPONSE_CODE, &http_code);
}