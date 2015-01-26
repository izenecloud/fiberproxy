#ifndef FIBER_HTTP_SERVER_V2_H
#define FIBER_HTTP_SERVER_V2_H

#include "HttpConnection.h"
#include "AsyncMultiIOServicesServer.h"

namespace fibp {

class FiberHttpServerV2
: public AsyncMultiIOServicesServer<HttpConnectionFactory>
{
public:
    typedef AsyncMultiIOServicesServer<HttpConnectionFactory> parent_type;
    typedef boost::shared_ptr<HttpConnectionFactory> factory_ptr;

    FiberHttpServerV2(
        const boost::asio::ip::tcp::endpoint& bindPort,
        factory_ptr& connectionFactory,
        std::size_t threadPoolSize
    );

    void run();
    void stop();

private:
    std::size_t threadPoolSize_;
    bool normal_stop_;
};

}

#endif // 
