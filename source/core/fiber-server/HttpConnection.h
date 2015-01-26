#ifndef FIBP_HTTP_CONNECTION_H
#define FIBP_HTTP_CONNECTION_H

#include "HttpProtocolHandler.h"
#include <util/driver/Router.h>
#include <util/driver/Reader.h>
#include <util/driver/Writer.h>
#include <util/driver/Poller.h>

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/function.hpp>

#include <string>
#include <vector>
#include <utility>
#include "FiberPool.hpp"

namespace fibp
{

class HttpConnection : public boost::enable_shared_from_this<HttpConnection>, private boost::noncopyable
{
public:
    typedef boost::shared_ptr<HttpConnection> connection_ptr;
    typedef boost::shared_ptr<http::session_t>  context_ptr;
    typedef boost::shared_ptr<izenelib::driver::Router> router_ptr;

    typedef boost::asio::ip::address ip_address;

    HttpConnection(boost::asio::io_service& s,
        const router_ptr& router, fiber_pool_ptr_t pool);

    ~HttpConnection();
    inline boost::asio::ip::tcp::socket& socket()
    {
        return socket_;
    }

    bool request_cb();
    void start();

private:
    void async_write_rsp(context_ptr context);
    void async_write_error_rsp(const std::string& rsp);
    context_ptr create_context();
    void after_write(context_ptr context);
    void shutdown();
    void onReadError(const boost::system::error_code& ec);
    void write_error_rsp(const std::string& rsp);
    void write_rsp(context_ptr context);

    bool handleRequestFunc(const std::string& controller, const std::string& action,
        context_ptr context);
    bool handleRequest(context_ptr context);
    context_ptr createContext();

    boost::asio::ip::tcp::socket socket_;
    izenelib::driver::Poller poller_;
    boost::scoped_ptr<izenelib::driver::Reader> reader_;
    boost::scoped_ptr<izenelib::driver::Writer> writer_;

    http::session_t next_context_;
    router_ptr router_;
    boost::shared_ptr<http::request_parser>  req_parser_;
    fiber_pool_ptr_t fiber_pool_;
};

class HttpConnectionFactory
{
public:
    typedef boost::shared_ptr<izenelib::driver::Router> router_ptr;

    void setFiberPoolMgr(FiberPoolMgr* fiberpool_mgr)
    {
        fiberpool_mgr_ = fiberpool_mgr;
    }

    HttpConnectionFactory(const router_ptr& router);
    typedef HttpConnection connection_type;
    inline HttpConnection* create(boost::asio::io_service& s)
    {
        fiber_pool_ptr_t pool;
        if (fiberpool_mgr_)
            pool = fiberpool_mgr_->getFiberPool();
        return new HttpConnection(s, router_, pool);
    }
private:
    router_ptr router_;
    FiberPoolMgr* fiberpool_mgr_;
};

}

#endif
