#ifndef WEB_FTP_HPP
#define WEB_FTP_HPP

#include <web/curl.hpp>

namespace web
{
    class ftp_curl : private curl
    {
    public:
        ftp_curl(const std::string &url, const std::string &credentials);

        void send_file(const std::string &file_name, const byte_buffer &buffer, bool should_reset = false);

    private:
        std::string url_;
        std::string credentials_;
    };
} // namespace web

#endif