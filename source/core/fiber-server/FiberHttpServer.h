#ifndef FIBER_HTTP_SERVER_H
#define FIBER_HTTP_SERVER_H

#include "HttpConnection.h"
#include <net/tcpserver/AsyncServer.h>

namespace fibp {

class FiberHttpServer
: public tcpserver::AsyncServer<HttpConnectionFactory>
{
public:
    typedef tcpserver::AsyncServer<HttpConnectionFactory> parent_type;
    typedef boost::shared_ptr<HttpConnectionFactory> factory_ptr;

    FiberHttpServer(
        const boost::asio::ip::tcp::endpoint& bindPort,
        factory_ptr& connectionFactory,
        std::size_t threadPoolSize
    );

    void run();
    void stop();

private:
    void worker(boost::barrier& b);
    void fiber_runner(const boost::thread::id& tid, boost::barrier& b);

    std::size_t threadPoolSize_;
    bool normal_stop_;
    FiberPoolMgr fiberpool_mgr_;
};

}

#endif // 
