#include "FibpPortForwardServer.h"
#include <glog/logging.h>

#include <boost/thread.hpp>
#include <boost/bind.hpp>

#include <iostream>

namespace fibp {

PortForwardServer::PortForwardServer(
    const boost::asio::ip::tcp::endpoint& bindPort,
    factory_ptr& connectionFactory,
    std::size_t threadPoolSize
)
: parent_type(bindPort, connectionFactory, threadPoolSize),
  threadPoolSize_(threadPoolSize)
{
    if (threadPoolSize_ < 2)
    {
        threadPoolSize_ = 2;
    }
    parent_type::listen();
}

void PortForwardServer::run()
{
    parent_type::asyncAccept();
    for(;;)
    {
        try
        {
            parent_type::run();
            break;
        }
        catch(const std::exception& e)
        {
            LOG(ERROR) << "driver server exception: " << e.what();
            parent_type::reset();
        }
    }
}

void PortForwardServer::stop()
{
    parent_type::stop();
}

}
