#include "FiberHttpServerV2.h"
#include <glog/logging.h>

#include <boost/thread.hpp>
#include <boost/bind.hpp>

#include <iostream>

namespace fibp {

FiberHttpServerV2::FiberHttpServerV2(
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
}

void FiberHttpServerV2::run()
{
    normal_stop_ = false;
    parent_type::init();
    for(;;)
    {
        try
        {
            parent_type::run();
            if (normal_stop_)
                break;
            else
            {
                parent_type::reset();
            }
        }
        catch(const std::exception& e)
        {
            LOG(ERROR) << "http server exception: " << e.what();
            parent_type::reset();
        }
    }
}

void FiberHttpServerV2::stop()
{
    normal_stop_ = true;
    //LOG(INFO) << "stop http server by request.";
    parent_type::stop();
}

}
