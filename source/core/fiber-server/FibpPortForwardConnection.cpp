#include "FibpPortForwardConnection.h"
#include "yield.hpp"
#include <forward-manager/FibpClient.h>

#include <boost/bind.hpp>
#include <iostream>
#include <log-manager/FibpLogger.h>
#include <glog/logging.h>

using namespace izenelib;
namespace fibp
{

static int s_guess_client_num = 0;

PortForwardConnection::PortForwardConnection(boost::asio::io_service& s,
    uint16_t forward_port, ForwardChainCB cb)
: socket_(s), forward_port_(forward_port), forward_cb_(cb)
{
    ++s_guess_client_num;
    if (s_guess_client_num % 10 == 0)
    {
        LOG(INFO) << "guessed forward client num : " << s_guess_client_num;
    }
}

PortForwardConnection::~PortForwardConnection()
{
    --s_guess_client_num;
}

void PortForwardConnection::start()
{
    std::string buf;
    buf.resize(10240);
    // prepare the forward connection at first.
    // Since we can not determine the data segment, 
    // we can not change the forward connection during data forwarding.
    ClientSessionPtr forward_client; 
    boost::system::error_code ec;
    ec = chain_forward_connection(forward_client);
    if (!forward_client)
    {
        LOG(ERROR) << "failed to get the forward connection." << ec.message();
        shutdown();
        return;
    }

    while(socket_.is_open())
    {

        std::size_t bytes_read = socket_.async_read_some(
            boost::asio::mutable_buffers_1(&(buf[0]), buf.size()),
            boost::fibers::asio::yield[ec]);

        if(ec)
        {
            LOG(INFO) << "reading data error: " << ec.message();
            break;
        }

        ec = forward_data(forward_client->socket_, buf.data(), bytes_read);

        if (ec)
        {
            LOG(INFO) << "reading data error: " << ec.message();
            break;
        }
    }
    LOG(INFO) << "connection closed.";
    forward_client->shutdown(false);
    shutdown();
}

boost::system::error_code PortForwardConnection::chain_forward_connection(ClientSessionPtr& forward_client)
{
    return forward_cb_(forward_port_, socket_, forward_client);
}

boost::system::error_code PortForwardConnection::forward_data(boost::asio::ip::tcp::socket& forward_socket,
    const char* data, std::size_t bytes)
{
    boost::system::error_code ec;
    boost::asio::async_write(forward_socket, boost::asio::const_buffers_1(data, bytes),
        boost::fibers::asio::yield[ec]);
    return ec;
}

void PortForwardConnection::shutdown()
{
    try
    {
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_receive);
        //LOG(INFO) << "closing client";
    }
    catch(const std::exception& e)
    {
        LOG(INFO) << e.what();
    }
}

}

