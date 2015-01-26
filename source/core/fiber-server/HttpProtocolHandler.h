#ifndef HTTP_PROTOCOL_HANDLER_H
#define HTTP_PROTOCOL_HANDLER_H

#include <util/driver/Request.h>
#include <util/driver/Response.h>
#include <util/ClockTimer.h>

#include <boost/function.hpp>
#include <boost/asio.hpp>
#include <boost/system/error_code.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>

#include <string>
#include <vector>
#include <utility>

namespace fibp
{
namespace http
{

enum method
{
    DELETE,
    GET,
    HEAD,
    POST,
    PUT
};
enum status_code
{
    OK = 200,
    BAD_REQUEST = 400,
    NOT_FOUND   = 404,
    INTERNAL_SERVER_ERROR = 500,
    NOT_IMPLEMENTED = 501,
    BAD_GATEWAY = 502,
    SERVICE_UNAVAILABLE = 503,
    GATEWAY_TIMEOUT = 504,
};
typedef std::pair<std::string, std::string> header_t;
typedef std::vector<header_t>  headers_t;
inline std::string find_header(const headers_t &headers, const std::string &key)
{
    headers_t::const_iterator it = headers.begin();
    for(; it != headers.end(); ++it)
    {
        if (it->first == key)
            return it->second;
    }
    return "";
}

struct request_t 
{
    short http_major_;
    short http_minor_;
    method method_;
    std::string schema_;
    std::string host_;
    int port_;
    std::string path_;
    std::string query_;
    headers_t headers_;
    bool keep_alive_;
    std::string body_;
    request_t()
        : http_major_(1), http_minor_(1),
        method_(POST), port_(0), keep_alive_(false)
    {
    }
    void clear()
    {
        http_major_ = 0;
        http_minor_ = 0;
        method_ = GET;
        schema_.clear();
        host_.clear();
        port_ = 0;
        path_.clear();
        query_.clear();
        headers_.clear();
        keep_alive_ = false;
        body_.clear();
    }
    void swap(request_t& other)
    {
        using std::swap;
        swap(http_major_, other.http_major_);
        swap(http_minor_, other.http_minor_);
        swap(method_, other.method_);
        swap(schema_, other.schema_);
        swap(host_, other.host_);
        swap(port_, other.port_);
        swap(path_, other.path_);
        swap(query_, other.query_);
        headers_.swap(other.headers_);
        swap(keep_alive_, other.keep_alive_);
        swap(body_, other.body_);
    }
};

struct response_t
{
    short http_major_;
    short http_minor_;
    status_code code_;
    std::string status_message_;
    headers_t headers_;
    bool keep_alive_;
    std::string body_;
    response_t()
        : http_major_(1), http_minor_(1), code_(GATEWAY_TIMEOUT), keep_alive_(false)
    {
    }
    void swap(response_t& other)
    {
        using std::swap;
        swap(http_major_, other.http_major_);
        swap(http_minor_, other.http_minor_);
        swap(code_, other.code_);
        swap(status_message_, other.status_message_);
        headers_.swap(other.headers_);
        swap(keep_alive_, other.keep_alive_);
        swap(body_, other.body_);
    }

    void clear()
    {
        http_major_ = 1;
        http_minor_ = 1;
        code_ = GATEWAY_TIMEOUT;
        status_message_.clear();
        headers_.clear();
        keep_alive_ = false;
        body_.clear();
    }
};

struct session_t
{
    request_t req_;
    response_t rsp_;
    izenelib::driver::Request jsonRequest_;
    izenelib::driver::Response jsonResponse_;
    izenelib::util::ClockTimer serverTimer_;
    //int count_;
    //int max_keepalive_;
    session_t()
    {
    }
    void swap(session_t& other)
    {
        using std::swap;
        req_.swap(other.req_);
        rsp_.swap(other.rsp_);
        jsonRequest_.swap(other.jsonRequest_);
        jsonResponse_.swap(other.jsonResponse_);
        swap(serverTimer_, other.serverTimer_);
    }
};
inline void swap(session_t& r, session_t& l)
{
    r.swap(l);
}

typedef boost::function<bool()> request_handler_t;
typedef boost::function<void()> close_handler_t;
typedef boost::function<void(const boost::system::error_code&)> read_error_handler_t;

std::ostream &operator<<(std::ostream &s, response_t &rsp);
// For client side
std::ostringstream &operator<<(std::ostringstream &s, const request_t &req);

namespace request
{
    class parser_impl;
}
namespace response
{
    class parser_impl;
}

class request_parser : public boost::enable_shared_from_this<request_parser>
{
public:
    request_parser(boost::asio::ip::tcp::socket& socket, session_t& s);
    ~request_parser();
    void init_handler(const request_handler_t& rcb, const close_handler_t& ccb, const read_error_handler_t& read_err_cb);
    bool start_parse();
    void do_parse();

private:
    boost::shared_ptr<request::parser_impl>  impl_;
};

class response_parser 
{
public:
    response_parser(boost::asio::ip::tcp::socket& socket, response_t& rsp);
    bool parse_response(boost::system::error_code& ec);
private:
    boost::shared_ptr<response::parser_impl> impl_;
};

} // namespace http
} // namespace fibp

#endif
