#ifndef FIBER_DRIVER_CONNECTION_H
#define FIBER_DRIVER_CONNECTION_H

#include <util/driver/DriverConnectionContext.h>
#include <util/driver/Router.h>
#include <util/driver/Reader.h>
#include <util/driver/Writer.h>
#include <util/driver/Poller.h>
#include "FiberPool.hpp"

#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/threadpool.hpp>
#include <boost/unordered_map.hpp>

namespace fibp {

class FiberDriverConnection
: public boost::enable_shared_from_this<FiberDriverConnection>,
  private boost::noncopyable
{
    typedef boost::shared_ptr<FiberDriverConnection> connection_ptr;
    typedef boost::shared_ptr<izenelib::driver::DriverConnectionContext> context_ptr;
    typedef boost::shared_ptr<izenelib::driver::Router> router_ptr;

    typedef boost::asio::ip::address ip_address;

public:
    FiberDriverConnection(
        boost::asio::io_service& ioService,
        const router_ptr& router,
        fiber_pool_ptr_t fiber_pool
    );
    ~FiberDriverConnection();

    /// @brief Socket used to communicate with client.
    inline boost::asio::ip::tcp::socket& socket()
    {
        return socket_;
    }

    /// @brief Start to handle request received from the connection.
    void start();

private:
    /// @brief Read next form size and sequence asynchronously.
    void asyncReadFormHeader();

    /// @brief Handler called after form header has been received.
    void afterReadFormHeader(const boost::system::error_code& e);

    /// @brief Read request header and body of next request form.
    void asyncReadFormPayload();

    /// @brief Handler called after request header and body has been received.
    void afterReadFormPayload(const boost::system::error_code& e);

    /// @brief Handle the request
    void handleRequest(context_ptr context,
                       const boost::system::error_code& e);
    void handleRequestFunc(context_ptr context);

    /// @brief Write response asynchronously.
    void asyncWriteResponse(context_ptr context);

    void asyncWriteError(const context_ptr& context,
                         const std::string& message);

    /// @brief Handler called after write.
    ///
    /// It keep a reference to this object and context.
    void afterWriteResponse(context_ptr);

    /// @brief Shutdown receive end.
    void shutdownReceive();

    /// @brief Create a new context from the buffer \c nextContext_
    context_ptr createContext();

    /// @brief Called on read error
    void onReadError(const boost::system::error_code& e);

    void writeError(context_ptr context, const std::string& errmsg);
    void writeRsp(context_ptr context);

    /// @brief Socket to communicate with the client in this connection.
    boost::asio::ip::tcp::socket socket_;

    izenelib::driver::Poller poller_;

    /// @brief Router used to look up associated action handler.
    router_ptr router_;

    /// @brief Buffer to read size and request body
    izenelib::driver::DriverConnectionContext nextContext_;

    /// @brief Request reader
    boost::scoped_ptr<izenelib::driver::Reader> reader_;

    /// @brief Response writer
    boost::scoped_ptr<izenelib::driver::Writer> writer_;

    // @brief size limit
    enum {
        kLimitSize = 64 * 1024 * 1024 // 64M
    };

    fiber_pool_ptr_t fiber_pool_;
};

class FiberDriverConnectionFactory
{
public:
    typedef boost::shared_ptr<izenelib::driver::Router> router_ptr;

    void setFiberPoolMgr(FiberPoolMgr* fiberpool_mgr)
    {
        fiberpool_mgr_ = fiberpool_mgr;
    }

    FiberDriverConnectionFactory(const router_ptr& router);
    typedef FiberDriverConnection connection_type;
    inline FiberDriverConnection* create(boost::asio::io_service& ioService)
    {
        fiber_pool_ptr_t pool;
        if (fiberpool_mgr_)
            pool = fiberpool_mgr_->getFiberPool();

        return new FiberDriverConnection(ioService, router_, pool);
    }

private:
    router_ptr router_;
    FiberPoolMgr* fiberpool_mgr_;
};

}

#endif // FIBER_DRIVER_CONNECTION_H
