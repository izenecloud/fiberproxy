#include "FibpPortForwardMgr.h"
#include "FibpServiceMgr.h"
#include <fiber-server/yield.hpp>
#include <forward-manager/FibpClient.h>
#include <fiber-server/FibpPortForwardServer.h>
#include <glog/logging.h>

namespace fibp
{

static void handle_forward_rsp(ClientSessionPtr client, boost::asio::ip::tcp::socket& src_socket)
{
    std::string buf;
    buf.resize(10240);
    boost::system::error_code ec;
    while(true)
    {
        std::size_t bytes_read = 0;
        ec = client->async_read_some(boost::asio::mutable_buffers_1(&(buf[0]), buf.size()),
           buf.size(), bytes_read);
        if (ec)
        {
            LOG(INFO) << "read from forward error: " << ec.message();
            break;
        }

        boost::asio::async_write(src_socket, boost::asio::const_buffers_1(buf.data(), bytes_read),
            boost::fibers::asio::yield[ec]);
        if (ec)
        {
            LOG(INFO) << "write back to forward error: " << ec.message();
            break;
        }
    }
    client->shutdown(false);
    LOG(INFO) << "forward connection closed.";
    src_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_receive, ec);
}

FibpPortForwardMgr::FibpPortForwardMgr(FibpServiceMgr* mgr)
    :fibp_service_mgr_(mgr)
{
}

boost::system::error_code FibpPortForwardMgr::create_forward_connection(uint16_t forward_port, 
    boost::asio::ip::tcp::socket& src_socket,
    ClientSessionPtr& forward_client)
{
    ForwardInfoT finfo;
    {
        boost::shared_lock<boost::shared_mutex> guard(mutex_);
        std::map<uint16_t, ForwardInfoT>::const_iterator it = forward_service_list_.find(forward_port);
        if (it == forward_service_list_.end())
        {
            LOG(INFO) << "No forword service at port: " << forward_port;
            return boost::system::errc::make_error_code(boost::system::errc::not_supported);
        }
        finfo = it->second;
    }
    boost::system::error_code ec;
    std::size_t balance_index = rand() % 1000;
    int retry = 3;
    while(--retry > 0)
    {
        std::string host, port;
        bool ret = fibp_service_mgr_->get_service_address(++balance_index, finfo.service_name,
            (ServiceType)finfo.service_type, host, port);
        if (!ret)
        {
            if (finfo.service_type == Custom_Service)
            {
                ret = fibp_service_mgr_->get_service_address(++balance_index, finfo.service_name,
                    HTTP_Service, host, port);
            }
            if (!ret)
            {
                LOG(INFO) << "No machines for the service: " << finfo.service_name << ", " <<
                    ", " << finfo.service_type;
                return boost::system::errc::make_error_code(boost::system::errc::not_supported);
            }
        }

        ClientSessionPtr client;
        client.reset(new ClientSession(src_socket.get_io_service(), host, port));
        client->set_timeout(0, 0);
        ec = client->async_connect();
        if (ec)
        {
            // try next host
            LOG(INFO) << "failed connect forward service: " << host << ":" << port;
            continue;
        }
        boost::fibers::fiber f(boost::bind(&handle_forward_rsp, client, boost::ref(src_socket)));
        f.detach();
        forward_client = client;
        return ec;
    }
    return ec;
}

void FibpPortForwardMgr::getAllForwardServices(std::vector<ForwardInfoT> &services)
{
    std::map<uint16_t, ForwardInfoT>::const_iterator it = forward_service_list_.begin();
    for (; it != forward_service_list_.end(); ++it)
    {
        services.push_back(it->second);
    }
}

bool FibpPortForwardMgr::getForwardService(uint16_t forward_port, ForwardInfoT& info)
{
    boost::unique_lock<boost::shared_mutex> guard(mutex_);
    std::map<uint16_t, ForwardInfoT>::const_iterator it = forward_service_list_.find(forward_port);
    if (it == forward_service_list_.end())
        return false;
    info = it->second;
    return true;
}

void FibpPortForwardMgr::updateForwardService(uint16_t forward_port,
    const std::string& service_name,
    int type)
{
    ForwardInfoT info;
    info.service_name = service_name;
    info.service_type = type;
    info.port = forward_port;

    boost::unique_lock<boost::shared_mutex> guard(mutex_);
    forward_service_list_[forward_port] = info;
    LOG(INFO) << "port :" << forward_port << " is forwarding to service: " << service_name;
}

void FibpPortForwardMgr::run_server(uint16_t port, boost::shared_ptr<PortForwardServer> server)
{
    try
    {
        server->run();
    }
    catch(const std::exception& e)
    {
        LOG(INFO) << "forward server exception at port: " << port;
        boost::unique_lock<boost::shared_mutex> guard(mutex_);
        forward_server_list_.erase(port);
    }
}

bool FibpPortForwardMgr::startPortForward(uint16_t &port)
{
    boost::unique_lock<boost::shared_mutex> guard(mutex_);
    boost::shared_ptr<PortForwardConnectionFactory> factory(
        new PortForwardConnectionFactory(0,
        boost::bind(&FibpPortForwardMgr::create_forward_connection, this, _1, _2, _3)));

    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(), 0);
    boost::shared_ptr<PortForwardServer> server;
    server.reset(new PortForwardServer(endpoint, factory, 4));
    port = server->getBindedEndpoint().port();
    factory->setBindedPort(port);
    forward_server_list_[port].server = server;
    forward_server_list_[port].running_thread.reset(new boost::thread(boost::bind(
            &FibpPortForwardMgr::run_server, this, port, server)));
    LOG(INFO) << "begin forward server at port:" << port;
    return true;
}

void FibpPortForwardMgr::stopPortForward(uint16_t port)
{
    ForwardServerInfo fsinfo;
    {
        boost::unique_lock<boost::shared_mutex> guard(mutex_);
        std::map<uint16_t, ForwardServerInfo>::iterator it = forward_server_list_.find(port);
        if (it == forward_server_list_.end())
        {
            return;
        }
        fsinfo = it->second;
        forward_server_list_.erase(it);

        forward_service_list_.erase(port);
    }
    fsinfo.server->stop();
    fsinfo.running_thread->join();
    LOG(INFO) << "port forward server stop at : " << port;
}

void FibpPortForwardMgr::stopAll()
{
    std::map<uint16_t, ForwardServerInfo> tmp_server_list;
    {
        boost::unique_lock<boost::shared_mutex> guard(mutex_);
        tmp_server_list.swap(forward_server_list_);
        forward_service_list_.clear();
    }
    std::map<uint16_t, ForwardServerInfo>::iterator it = tmp_server_list.begin();
    while (it != tmp_server_list.end())
    {
        it->second.server->stop();
        it->second.running_thread->join();
        ++it;
    }
    tmp_server_list.clear();
}

}
