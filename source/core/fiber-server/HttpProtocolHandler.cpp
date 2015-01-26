#include "HttpProtocolHandler.h"
#include <util/ClockTimer.h>
#include <util/driver/writers/JsonWriter.h>
#include <util/driver/readers/JsonReader.h>
#include <iostream>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/bind.hpp>
#include <boost/fiber/all.hpp>
#include "yield.hpp"
#include "http_parser.h"

namespace fibp
{

namespace http
{

static const std::string SERVER_NAME("FibpServer 1.0");
static const std::string METHOD_STRING[5] = 
{
    "DELETE",
    "GET",
    "HEAD",
    "POST",
    "PUT"
};

namespace request
{
struct parser_impl : public boost::enable_shared_from_this<parser_impl>
{
    enum parser_state
    {
        none,
        start,
        url,
        field,
        value,
        body,
        end
    };
    request_t &req() {return session_.req_; }
    int on_msg_begin()
    {
        session_.serverTimer_.restart();
        req().clear();
        url_.clear();
        state_ = start;
        return 0;
    }
    int on_url(const char* at, size_t len)
    {
        if (state_ == url)
            url_.append(at, len);
        else
        {
            url_.reserve(1024);
            url_.assign(at, len);
        }
        state_ = url;
        return 0;
    }
    int on_status_complete()
    {
        return 0;
    }

    int on_header_field(const char *at, size_t length) {
        if (state_==field) {
            req().headers_.rbegin()->first.append(at, length);
        } else {
            req().headers_.push_back(header_t());
            req().headers_.rbegin()->first.reserve(256);
            req().headers_.rbegin()->first.assign(at, length);
        }
        state_=field;
        return 0;
    }

    int on_header_value(const char *at, size_t length) {
        if (state_==value)
            req().headers_.rbegin()->second.append(at, length);
        else {
            req().headers_.rbegin()->second.reserve(256);
            req().headers_.rbegin()->second.assign(at, length);
        }
        state_=value;
        return 0;
    }

    int on_headers_complete() {
        return 0;
    }

    int on_body(const char *at, size_t length) {
        req().body_.append(at, length);
        state_ = body;
        return 0;
    }

    int on_msg_complete() 
    {                                                
        state_=end;
        req().method_ = (method)(parser_.method);
        req().http_major_ = parser_.http_major;
        req().http_minor_ = parser_.http_minor;
        http_parser_url u;
        http_parser_parse_url(url_.c_str(),
            url_.size(),
            0,
            &u);
        // Components for proxy requests
        // NOTE: host, and port may only exist in proxy requests
        if(u.field_set & 1 << UF_HOST) {
            req().host_ = std::string(url_.begin()+u.field_data[UF_HOST].off,
                    url_.begin()+u.field_data[UF_HOST].off+u.field_data[UF_HOST].len);
        }
        if(u.field_set & 1 << UF_PORT) {
            req().port_ = u.port;
        } else {
            req().port_ = 0;
        }

        // Common components
        if(u.field_set & 1 << UF_PATH) {
            req().path_ = std::string(url_.begin()+u.field_data[UF_PATH].off,
                    url_.begin()+u.field_data[UF_PATH].off+u.field_data[UF_PATH].len);
        }
        if(u.field_set & 1 << UF_QUERY) {
            req().query_ = std::string(url_.begin()+u.field_data[UF_QUERY].off,
                    url_.begin()+u.field_data[UF_QUERY].off+u.field_data[UF_QUERY].len);
        }
        req().keep_alive_ = http_should_keep_alive(&parser_);
        return (should_continue_ = cb_()) ? 0 : -1;
    }

    static int on_msg_begin(http_parser*p) {
        return reinterpret_cast<parser_impl *>(p->data)->on_msg_begin();
    }
    static int on_url(http_parser*p, const char *at, size_t length) {
        return reinterpret_cast<parser_impl *>(p->data)->on_url(at, length);
    }
    static int on_status_complete(http_parser*p, const char *at, size_t length) {
        return reinterpret_cast<parser_impl *>(p->data)->on_status_complete();
    }
    static int on_header_field(http_parser*p, const char *at, size_t length) {
        return reinterpret_cast<parser_impl *>(p->data)->on_header_field(at, length);
    }
    static int on_header_value(http_parser*p, const char *at, size_t length) {
        return reinterpret_cast<parser_impl *>(p->data)->on_header_value(at, length);
    }
    static int on_headers_complete(http_parser*p) {
        return reinterpret_cast<parser_impl *>(p->data)->on_headers_complete();
    }
    static int on_body(http_parser*p, const char *at, size_t length) {
        return reinterpret_cast<parser_impl *>(p->data)->on_body(at, length);
    }
    static int on_msg_complete(http_parser*p) {
        return reinterpret_cast<parser_impl *>(p->data)->on_msg_complete();
    }

    static http_parser_settings settings_;

    boost::asio::ip::tcp::socket& socket_;
    session_t &session_;
    http_parser parser_;
    std::string url_;
    request_handler_t  cb_;
    close_handler_t close_cb_;
    read_error_handler_t read_error_cb_;
    parser_state  state_;
    bool should_continue_;
    static const int buf_size_ = 1024;
    char read_buf_[buf_size_];

    parser_impl(boost::asio::ip::tcp::socket& socket, session_t &session)
        : socket_(socket), session_(session)
    {
        parser_.data = reinterpret_cast<void*>(this);
        http_parser_init(&parser_, HTTP_REQUEST);
    }

    void init_handler(const request_handler_t &cb, const close_handler_t& close_cb,
        const read_error_handler_t& read_error_cb)
    {
        cb_ = cb;
        close_cb_ = close_cb;
        read_error_cb_ = read_error_cb;
    }

    bool read_some_handler(const boost::system::error_code& ec, std::size_t bytes_transferred)
    {
        if (ec && ec != boost::asio::error::eof)
        {
            read_error_cb_(ec);
            http_parser_init(&parser_, HTTP_REQUEST);
            return false;
        }
        std::size_t nparsed = 0;
        nparsed = http_parser_execute(&parser_, &settings_, read_buf_, bytes_transferred);
        if (bytes_transferred <= 0)
        {
            //std::cerr << "client request to close." << std::endl;
            // closed
            close_cb_();
            http_parser_init(&parser_, HTTP_REQUEST);
            return false;
        }

        if (!should_continue_)
        {
            http_parser_init(&parser_, HTTP_REQUEST);
            return false;
        }
        if (nparsed != bytes_transferred)
        {
            close_cb_();
            http_parser_init(&parser_, HTTP_REQUEST);
            return false;
        }
        socket_.async_read_some(
            boost::asio::mutable_buffers_1(read_buf_, buf_size_),
            boost::bind(&parser_impl::read_some_handler, shared_from_this(), _1, _2));
        return true;
    }

    bool start_parse()
    {
        should_continue_ = true;
        state_ = none;

        socket_.async_read_some(
            boost::asio::mutable_buffers_1(read_buf_, buf_size_),
            boost::bind(&parser_impl::read_some_handler, shared_from_this(), _1, _2));

        return true;
    }

    void do_parse()
    {
        should_continue_ = true;
        state_ = none;

        while(true)
        {
            boost::system::error_code ec;
            std::size_t bytes_transferred = 0;
            bytes_transferred = socket_.async_read_some(
                boost::asio::mutable_buffers_1(read_buf_, buf_size_),
                boost::fibers::asio::yield[ec]);

            if (ec && ec != boost::asio::error::eof)
            {
                read_error_cb_(ec);
                break;
            }
            std::size_t nparsed = 0;
            nparsed = http_parser_execute(&parser_, &settings_, read_buf_, bytes_transferred);
            if (bytes_transferred <= 0)
            {
                //std::cerr << "client request to close." << std::endl;
                // closed
                close_cb_();
                break;
            }

            if (!should_continue_)
            {
                break;
            }

            if (nparsed != bytes_transferred)
            {
                close_cb_();
                break;
            }
        }
        http_parser_init(&parser_, HTTP_REQUEST);
    }
};

http_parser_settings parser_impl::settings_ = {
    &parser_impl::on_msg_begin,
    &parser_impl::on_url,
    &parser_impl::on_status_complete,
    &parser_impl::on_header_field,
    &parser_impl::on_header_value,
    &parser_impl::on_headers_complete,
    &parser_impl::on_body,
    &parser_impl::on_msg_complete,
};

} // end of namespace request

request_parser::request_parser(boost::asio::ip::tcp::socket& socket, session_t& s)
: impl_(new request::parser_impl(socket, s))
{
}

request_parser::~request_parser()
{
    impl_.reset();
}

void request_parser::init_handler(const request_handler_t& rcb, const close_handler_t& ccb, const read_error_handler_t& read_err_cb)
{
    impl_->init_handler(rcb, ccb, read_err_cb);
}

bool request_parser::start_parse()
{
    return impl_->start_parse();
}

void request_parser::do_parse()
{
    return impl_->do_parse();
}



namespace response
{
struct parser_impl
{
    enum parser_state
    {
        none,
        start,
        status,
        field,
        value,
        body,
        end
    };

    response_t &resp() { return response_; }

    int on_msg_begin() {
        resp().clear();
        state_=start;
        return 0;
    }
    int on_url(const char *at, size_t length) {
        return 0;
    }
    int on_status_complete(const char* at, size_t length) {
        if (state_ == status)
            resp().status_message_.append(at, length);
        else
        {
            resp().status_message_.assign(at, length);
        }
        state_ = status;
        return 0;
    }
    int on_header_field(const char *at, size_t length) {
        if (state_==field) {
            resp().headers_.rbegin()->first.append(at, length);
        } else {
            resp().headers_.push_back(header_t());
            resp().headers_.rbegin()->first.reserve(256);
            resp().headers_.rbegin()->first.assign(at, length);
        }
        state_=field;
        return 0;
    }

    int on_header_value(const char *at, size_t length) {
        if (state_==value)
            resp().headers_.rbegin()->second.append(at, length);
        else {
            resp().headers_.rbegin()->second.reserve(256);
            resp().headers_.rbegin()->second.assign(at, length);
        }
        state_=value;
        return 0;
    }
    int on_headers_complete() {
        return 0;
    }
    int on_body(const char *at, size_t length) {
        resp().body_.append(at, length);
        state_=body;
        return 0;
    }
    int on_msg_complete() {
        state_=end;
        resp().http_major_ = parser_.http_major;
        resp().http_minor_ = parser_.http_minor;
        resp().code_ = status_code(parser_.status_code);
        resp().keep_alive_ = http_should_keep_alive(&parser_);
        should_continue_=false;
        return 0;
    }

    static int on_msg_begin(http_parser*p) {
        return reinterpret_cast<parser_impl *>(p->data)->on_msg_begin();
    }
    static int on_url(http_parser*p, const char *at, size_t length) {
        return reinterpret_cast<parser_impl *>(p->data)->on_url(at, length);
    }
    static int on_status_complete(http_parser*p, const char* at, size_t length) {
        return reinterpret_cast<parser_impl *>(p->data)->on_status_complete(at, length);
    }
    static int on_header_field(http_parser*p, const char *at, size_t length) {
        return reinterpret_cast<parser_impl *>(p->data)->on_header_field(at, length);
    }
    static int on_header_value(http_parser*p, const char *at, size_t length) {
        return reinterpret_cast<parser_impl *>(p->data)->on_header_value(at, length);
    }
    static int on_headers_complete(http_parser*p) {
        return reinterpret_cast<parser_impl *>(p->data)->on_headers_complete();
    }
    static int on_body(http_parser*p, const char *at, size_t length) {
        return reinterpret_cast<parser_impl *>(p->data)->on_body(at, length);
    }
    static int on_msg_complete(http_parser*p) {
        return reinterpret_cast<parser_impl *>(p->data)->on_msg_complete();
    }

    static http_parser_settings settings_;

    boost::asio::ip::tcp::socket &is_;
    response_t &response_;
    http_parser parser_;
    std::string url_;
    parser_state state_;
    bool should_continue_;
    static const int buf_size = 1024;
    char buf_[buf_size];
    parser_impl(boost::asio::ip::tcp::socket &is, response_t& resp);
    bool parse(boost::system::error_code& ec);
};

http_parser_settings parser_impl::settings_ = {
    &parser_impl::on_msg_begin,
    &parser_impl::on_url,
    &parser_impl::on_status_complete,
    &parser_impl::on_header_field,
    &parser_impl::on_header_value,
    &parser_impl::on_headers_complete,
    &parser_impl::on_body,
    &parser_impl::on_msg_complete,
};

parser_impl::parser_impl(boost::asio::ip::tcp::socket &is, response_t &resp)
:is_(is), response_(resp)
{
    parser_.data = reinterpret_cast<void*>(this);
    http_parser_init(&parser_, HTTP_RESPONSE);
}

bool parser_impl::parse(boost::system::error_code& ec)
{
    should_continue_ = true;
    state_ = none;
    int recved = 0;
    int nparsed = 0;
    while(is_.is_open())
    {
        recved = is_.async_read_some(boost::asio::mutable_buffers_1(buf_, buf_size),
            boost::fibers::asio::yield[ec]);
        if (ec && ec != boost::asio::error::eof)
        {
            http_parser_init(&parser_, HTTP_RESPONSE);
            return false;
        }
        if (recved < 0)
        {
            http_parser_init(&parser_, HTTP_RESPONSE);
            return false;
        }
        nparsed = http_parser_execute(&parser_, &settings_, buf_, recved);
        if (recved == 0)
        {
            // closed
            http_parser_init(&parser_, HTTP_RESPONSE);
            return true;
        }

        if (!should_continue_)
        {
            if (!resp().keep_alive_)
            {
                http_parser_init(&parser_, HTTP_RESPONSE);
            }
            break;
        }
        if (nparsed != recved)
        {
            std::cerr << http_errno_description(HTTP_PARSER_ERRNO(&parser_)) << std::endl;
            http_parser_init(&parser_, HTTP_RESPONSE);
            return false;
        }
    }
    return true;
}

} // namespace response

std::ostream &operator<<(std::ostream &s, response_t &resp)
{
    char buf[10];
    sprintf(buf, "%lu", resp.body_.size());
    s << "HTTP/1.1 " << resp.code_ << " ";
    if (!resp.status_message_.empty())
    {
        s << resp.status_message_;
    }
    else
    {
        s << " " << "\r\n";
    }
    bool server_found = false;
    bool content_len_found = false;
    for(std::size_t i = 0; i < resp.headers_.size(); ++i)
    {
        const header_t& h = resp.headers_[i];
        s << h.first << ": " << h.second << "\r\n";
        if (boost::algorithm::iequals(h.first, "server")) server_found = true;
        else if (boost::algorithm::iequals(h.first, "content-length")) content_len_found = true;
    }
    if (!server_found)
    {
        s << "Server: " << SERVER_NAME << "\r\n";
    }
    if (!content_len_found)
    {
        s << "Content-Length: " << buf << "\r\n";
    }
    s << "\r\n" << resp.body_;
    return s;
}

// Client side
std::ostringstream &operator<<(std::ostringstream &s, const request_t &req)
{
    s << METHOD_STRING[req.method_] << " " << req.path_;
    if (!req.query_.empty())
    {
        s << "?" << req.query_;
    }
    s << " HTTP/" << req.http_major_ << "." << req.http_minor_ << "\r\n";
    for(std::size_t i = 0; i < req.headers_.size(); ++i)
    {
        s << req.headers_[i].first << ": " << req.headers_[i].second << "\r\n";
    }
    s << "Content-Length: " << req.body_.size() << "\r\n";
    if (req.keep_alive_)
    {
        s << "Connection: Keep-Alive\r\n";
    }
    else
    {
        s << "Connection: close\r\n";
    }
    s << "\r\n";
    if (!req.body_.empty())
    {
        s << req.body_;
    }
    return s;
}

response_parser::response_parser(boost::asio::ip::tcp::socket& socket, response_t& rsp)
    : impl_(new response::parser_impl(socket, rsp))
{
}

bool response_parser::parse_response(boost::system::error_code& ec)
{
    return impl_->parse(ec);
}

} // namespace http

}
