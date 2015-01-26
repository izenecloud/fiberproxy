#ifndef FIBER_DRIVER_SERVER_V2_H
#define FIBER_DRIVER_SERVER_V2_H

#include "FiberDriverConnection.h"
#include "AsyncMultiIOServicesServer.h"

namespace fibp {

class FiberDriverServerV2
: public AsyncMultiIOServicesServer<FiberDriverConnectionFactory>
{
public:
    typedef AsyncMultiIOServicesServer<FiberDriverConnectionFactory> parent_type;
    typedef boost::shared_ptr<FiberDriverConnectionFactory> factory_ptr;

    FiberDriverServerV2(
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
