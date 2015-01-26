#ifndef FIBP_PORT_FORWARD_SERVER_H
#define FIBP_PORT_FORWARD_SERVER_H

#include "AsyncMultiIOServicesServer.h"
#include "FibpPortForwardConnection.h"

namespace fibp
{

class PortForwardServer
    : public AsyncMultiIOServicesServer<PortForwardConnectionFactory>
{
public:
    typedef AsyncMultiIOServicesServer<PortForwardConnectionFactory> parent_type;
    typedef boost::shared_ptr<PortForwardConnectionFactory> factory_ptr;
    PortForwardServer(const boost::asio::ip::tcp::endpoint& bindPort,
        factory_ptr& connectionFactory, std::size_t poolsize);

    void run();
    void stop();

private:
    std::size_t threadPoolSize_;
};

}

#endif
