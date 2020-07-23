#include <web/curl.hpp>

using namespace web;

namespace
{
    void curl_deleter(CURL *c)
    {
        curl_easy_cleanup(c);
    }
    curl_ptr make_curl_ptr(CURL *c)
    {
        return curl_ptr(c, curl_deleter);
    }

    void curl_slist_deleter(curl_slist *cs)
    {
        curl_slist_free_all(cs);
    }
    curl_slist_ptr make_curl_slist_ptr(curl_slist *cs)
    {
        return curl_slist_ptr(cs, curl_slist_deleter);
    }
} // namespace

curl_global_handle::curl_global_handle()
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}
curl_global_handle::~curl_global_handle()
{
    curl_global_cleanup();
}

curl::curl_error::curl_error(const std::string &what) : std::runtime_error(what) {}

curl::curl()
{
    this->curl_wrapper_ = make_curl_ptr(curl_easy_init());

    if (!this->curl_wrapper_)
    {
        throw curl::curl_error("Couldn't load curl.");
    }
}

curl::~curl() {}

std::string curl::get(const std::string &url, bool should_reset)
{
    CURLcode res;
    size_t http_code;

    if (should_reset)
        curl_easy_reset(this->curl_wrapper_.get());
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_LOW_SPEED_TIME, 60L);  // timeout period
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_LOW_SPEED_LIMIT, 30L); // number of bytes during timeout period
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_WRITEFUNCTION, curl::write_response);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_WRITEDATA, this);

    this->response_.str("");
    res = curl_easy_perform(this->curl_wrapper_.get());
    if (res != CURLE_OK)
    {
        throw curl::curl_error(curl_easy_strerror(res));
    }
    curl_easy_getinfo(this->curl_wrapper_.get(), CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code == 400)
    {
        throw curl::curl_error("Bad Request.");
    }
    else if (http_code == 500)
    {
        throw curl::curl_error("Server Error.");
    }

    return this->response_.str();
}

std::string curl::post(const std::string &url, bool should_reset)
{
    CURLcode res;
    size_t http_code;

    if (should_reset)
        curl_easy_reset(this->curl_wrapper_.get());
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_POSTFIELDSIZE, 0L);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_LOW_SPEED_TIME, 60L);  // timeout period
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_LOW_SPEED_LIMIT, 30L); // number of bytes during timeout period
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_WRITEFUNCTION, curl::write_response);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_WRITEDATA, this);

    this->response_.str("");
    res = curl_easy_perform(this->curl_wrapper_.get());
    if (res != CURLE_OK)
    {
        throw curl::curl_error(curl_easy_strerror(res));
    }
    curl_easy_getinfo(this->curl_wrapper_.get(), CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code == 400)
    {
        throw curl::curl_error("Bad Request.");
    }
    else if (http_code == 500)
    {
        throw curl::curl_error("Server Error.");
    }

    return this->response_.str();
}

std::string curl::post(const std::string &url, const std::string &body, bool should_reset)
{
    CURLcode res;
    size_t http_code;

    this->response_.str("");
    this->request_.str(body);
    this->request_size_ = body.size();

    if (should_reset)
        curl_easy_reset(this->curl_wrapper_.get());
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_URL, url.c_str());
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_POST, 1L);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_POSTFIELDSIZE, (long)this->request_size_);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_LOW_SPEED_TIME, 60L);  // timeout period
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_LOW_SPEED_LIMIT, 30L); // number of bytes during timeout period
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_SSL_VERIFYHOST, 2L);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_SSL_VERIFYPEER, 1L);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_WRITEFUNCTION, curl::write_response);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_WRITEDATA, this);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_READFUNCTION, curl::write_request);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_READDATA, this);

    res = curl_easy_perform(this->curl_wrapper_.get());
    if (res != CURLE_OK)
    {
        throw curl::curl_error(curl_easy_strerror(res));
    }
    curl_easy_getinfo(this->curl_wrapper_.get(), CURLINFO_RESPONSE_CODE, &http_code);

    if (http_code == 400)
    {
        throw curl::curl_error("Bad Request.");
    }
    else if (http_code == 500)
    {
        throw curl::curl_error("Server Error.");
    }

    return this->response_.str();
}

std::string curl::post_text(const std::string &url, const std::string &text)
{
    curl_slist *ptr = nullptr;

    ptr = curl_slist_append(ptr, "Accept: application/json");
    ptr = curl_slist_append(ptr, "Content-Type: text/plain");
    ptr = curl_slist_append(ptr, "charsets: utf-8");

    curl_slist_ptr headers = make_curl_slist_ptr(ptr);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_HTTPHEADER, headers.get());

    return this->post(url, text, false);
}

std::string curl::post_json(const std::string &url, const std::string &json_string)
{
    curl_slist *ptr = nullptr;

    ptr = curl_slist_append(ptr, "Accept: application/json");
    ptr = curl_slist_append(ptr, "Content-Type: application/json");
    ptr = curl_slist_append(ptr, "charsets: utf-8");

    curl_slist_ptr headers = make_curl_slist_ptr(ptr);
    curl_easy_setopt(this->curl_wrapper_.get(), CURLOPT_HTTPHEADER, headers.get());

    return this->post(url, json_string, false);
}

std::string curl::url_encode(const std::string &url)
{
    return curl_easy_escape(this->curl_wrapper_.get(), url.data(), url.size());
}

size_t curl::write_response(void *buffer, size_t size, size_t nmemb, void *userp)
{
    curl *c = static_cast<curl *>(userp);
    size_t buffer_size = size * nmemb;

    c->response_.write(static_cast<const char *>(buffer), buffer_size);
    return buffer_size;
}

size_t curl::write_request(void *buffer, size_t size, size_t nmemb, void *userp)
{
    curl *c = static_cast<curl *>(userp);
    size_t buffer_size = size * nmemb;
    size_t size_to_copy;

    if (c->request_size_)
    {
        size_to_copy = c->request_size_;
        if (size_to_copy > buffer_size)
        {
            size_to_copy = buffer_size;
        }

        c->request_.read((char *)buffer, size_to_copy);

        c->request_size_ -= size_to_copy;
        return size_to_copy;
    }

    return 0;
}