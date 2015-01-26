#ifndef FIBP_PORT_FORWARD_CONNECTION_H
#define FIBP_PORT_FORWARD_CONNECTION_H

#include <boost/enable_shared_from_this.hpp>
#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <boost/function.hpp>
#include <string>

namespace fibp
{

class ClientSession;
class PortForwardConnection : public boost::enable_shared_from_this<PortForwardConnection>,
    private boost::noncopyable
{
public:
    typedef boost::shared_ptr<PortForwardConnection> connection_ptr;
    typedef boost::function<boost::system::error_code(uint16_t, boost::asio::ip::tcp::socket&, boost::shared_ptr<ClientSession>&)> ForwardChainCB;

    PortForwardConnection(boost::asio::io_service& s, uint16_t forward_port, ForwardChainCB cb);

    ~PortForwardConnection();
    inline boost::asio::ip::tcp::socket& socket()
    {
        return socket_;
    }

    void start();

private:
    boost::system::error_code chain_forward_connection(boost::shared_ptr<ClientSession>& forward_client);
    boost::system::error_code forward_data(boost::asio::ip::tcp::socket& forward_socket,
        const char* data, std::size_t bytes);

    void shutdown();
    boost::asio::ip::tcp::socket socket_;
    uint16_t forward_port_;
    ForwardChainCB forward_cb_;
};

class PortForwardConnectionFactory
{
public:
    PortForwardConnectionFactory(uint16_t forward_port, PortForwardConnection::ForwardChainCB cb)
        : forward_port_(forward_port), forward_cb_(cb)
    {
    }
    void setBindedPort(uint16_t forward_port)
    {
        forward_port_ = forward_port;
    }
    typedef PortForwardConnection connection_type;
    inline PortForwardConnection* create(boost::asio::io_service& s)
    {
        return new PortForwardConnection(s, forward_port_, forward_cb_);
    }

private:
    uint16_t forward_port_;
    PortForwardConnection::ForwardChainCB forward_cb_;
};

}

#endif
