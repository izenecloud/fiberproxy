#ifndef FIBP_PORT_FORWARD_MGR_H
#define FIBP_PORT_FORWARD_MGR_H

#include <forward-manager/FibpClient.h>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <map>
#include <string>
#include <boost/thread.hpp>

namespace fibp {

class PortForwardServer;
class FibpServiceMgr;
class FibpPortForwardMgr
{
public:
    FibpPortForwardMgr(FibpServiceMgr*);
    boost::system::error_code create_forward_connection(uint16_t forward_port, 
        boost::asio::ip::tcp::socket& src_socket,
        ClientSessionPtr& forward_client);

    void updateForwardService(uint16_t forward_port, const std::string& service_name,
        int type);

    bool startPortForward(uint16_t &port);
    void stopPortForward(uint16_t port);
    void stopAll();
    bool getForwardService(uint16_t forward_port, ForwardInfoT &info);
    void getAllForwardServices(std::vector<ForwardInfoT>& services);

private:

    void run_server(uint16_t port, boost::shared_ptr<PortForwardServer> server);
    struct ForwardServerInfo
    {
        boost::shared_ptr<PortForwardServer> server;
        boost::shared_ptr<boost::thread> running_thread; 
    };
    std::map<uint16_t, ForwardInfoT>  forward_service_list_;
    std::map<uint16_t, ForwardServerInfo>  forward_server_list_;
    FibpServiceMgr* fibp_service_mgr_;
    boost::shared_mutex   mutex_;
};

}
#endif
