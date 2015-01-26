
#ifndef FIBP_CLIENT_H
#define FIBP_CLIENT_H

#include <common/FibpCommonTypes.h>
#include <fiber-server/HttpProtocolHandler.h>
#include <boost/shared_ptr.hpp>
#include <string>
#include <boost/asio.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/deadline_timer.hpp>
#include <ostream>

namespace msgpack
{
    class unpacker;
    class sbuffer;
}

namespace boost
{
namespace fibers
{
    class fiber;
}
}

namespace fibp
{

class FibpClientFuture;
typedef boost::shared_ptr<FibpClientFuture> FibpClientFuturePtr;

class ClientSession
{
public:
    ClientSession(boost::asio::io_service& service, const std::string& host, const std::string& port);
    bool send_data(const std::string& reqdata);
    void set_timeout(int conn_to_ms, int read_to_ms)
    {
        conn_to_ = conn_to_ms;
        read_to_ = read_to_ms;
    }
    void shutdown(bool need_close);
    std::string host()
    {
        return host_;
    }
    std::string port()
    {
        return port_;
    }
    boost::asio::io_service& get_io_service()
    {
        return io_;
    }

    boost::system::error_code async_read(const boost::asio::mutable_buffers_1& buf, std::size_t bufsize);

    boost::system::error_code async_read_some(const boost::asio::mutable_buffers_1& buf,
        std::size_t bufsize, std::size_t& bytes_read);
    void clear_timeout();
    void prepare_timeout(int sec);
    boost::system::error_code async_connect();

    boost::asio::ip::tcp::socket socket_;
    int conn_to_;
    int read_to_;

private:
    bool async_write(const std::string& reqdata);
    void check_deadline(const boost::system::error_code& ec);

    boost::asio::io_service& io_;
    boost::asio::deadline_timer  deadline_;
    std::string host_;
    std::string port_;
    bool connecting_;
};

typedef boost::shared_ptr<ClientSession> ClientSessionPtr;

class FibpClientBase
{
public:
    FibpClientBase(boost::asio::io_service& service, const std::string& host, const std::string& port);
    virtual ~FibpClientBase(){}
    virtual FibpClientFuturePtr send_request(
        const std::string& path, // used for http only.
        const std::string& reqdata,
        int timeout_ms) = 0;
    virtual bool get_response(FibpClientFuturePtr f, std::string& rsp, bool& can_retry) = 0;
    virtual ServiceType get_type() const = 0;
    std::string host() const;
    std::string port() const;
    //bool can_retry() const;

protected:
    ClientSessionPtr session_;
    //bool can_retry_;
};
typedef boost::shared_ptr<FibpClientBase> FibpClientBasePtr;

// http client is special so do not derived from base client.
class FibpHttpClient
{
public:
    FibpHttpClient(boost::asio::io_service& service, const std::string& host, const std::string& port);
    bool send_request(
        const std::string& path,
        http::method method,
        const std::string& reqdata,
        int timeout_ms);
    bool get_response(std::string& rsp);
    ServiceType get_type() const
    {
        return HTTP_Service;
    }

    std::string host() const;
    std::string port() const;
    bool send_http_request(
        http::request_t& http_req,
        int timeout_ms);
    bool get_http_response(http::response_t& http_rsp);

    bool can_retry() const
    {
        return can_retry_;
    }

    ClientSessionPtr session_;
    std::ostringstream request_stream_;
    http::response_t next_rsp_;
    http::response_parser rsp_parser_;
    bool can_retry_;
};
typedef boost::shared_ptr<FibpHttpClient> FibpHttpClientPtr;

class FibpRpcClient : public FibpClientBase
{
public:
    FibpRpcClient(boost::asio::io_service& service, const std::string& host, const std::string& port);
    ~FibpRpcClient();
    FibpClientFuturePtr send_request(
        const std::string& path,
        const std::string& reqdata,
        int timeout_ms);
    bool get_response(FibpClientFuturePtr f, std::string& rsp, bool& can_retry);
    ServiceType get_type() const
    {
        return RPC_Service;
    }
private:
    void set_response(uint32_t msgid, const std::string& rsp, bool is_success, bool can_retry);
    void set_all_error(const std::string& errinfo);
    void handle_read();
    uint32_t fid_;
    boost::shared_ptr<msgpack::unpacker> rpc_pac_;
    boost::shared_ptr<msgpack::sbuffer> rpc_buf_;
    typedef std::map<uint32_t, FibpClientFuturePtr> FutureListT;
    FutureListT future_list_;
    boost::shared_ptr<boost::fibers::fiber> reading_fiber_;
};

class FibpRawClient : public FibpClientBase
{
public:
    FibpRawClient(boost::asio::io_service& service, const std::string& host, const std::string& port);
    ~FibpRawClient();
    FibpClientFuturePtr send_request(
        const std::string& path,
        const std::string& reqdata,
        int timeout_ms);
    bool get_response(FibpClientFuturePtr f, std::string& rsp, bool& can_retry);
    ServiceType get_type() const
    {
        return Raw_Service;
    }

private:
    void set_response(uint32_t msgid, const std::string& rsp, bool is_success, bool can_retry);
    void set_all_error(const std::string& errinfo);
    void handle_read();
    bool readRpsHeader(std::string& rsp);
    bool afterReadRspHeader(const boost::system::error_code& ec,
        const char* header, std::string& rsp);
    bool readRspBody(std::string& rsp);
    bool afterReadRspBody(const boost::system::error_code& ec,
        std::string& rsp);
    uint32_t fid_;
    typedef std::map<uint32_t, FibpClientFuturePtr> FutureListT;
    FutureListT future_list_;
    boost::shared_ptr<boost::fibers::fiber> reading_fiber_;
};

}


#endif
