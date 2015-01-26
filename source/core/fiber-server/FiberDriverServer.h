#ifndef FIBER_DRIVER_SERVER_H
#define FIBER_DRIVER_SERVER_H

#include "FiberDriverConnection.h"
#include <net/tcpserver/AsyncServer.h>

namespace fibp {

class FiberDriverServer
: public tcpserver::AsyncServer<FiberDriverConnectionFactory>
{
public:
    typedef tcpserver::AsyncServer<FiberDriverConnectionFactory> parent_type;
    typedef boost::shared_ptr<FiberDriverConnectionFactory> factory_ptr;

    FiberDriverServer(
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
