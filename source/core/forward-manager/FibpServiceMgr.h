#ifndef FIBP_SERVICE_MGR_H
#define FIBP_SERVICE_MGR_H

#include <common/FibpCommonTypes.h>
#include <3rdparty/rapidjson/document.h>
#include <string>
#include <utility>
#include <map>
#include <set>
#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/thread.hpp>

namespace fibp
{

class FibpHttpClient;
class FibpServiceMgr
{
public:
    FibpServiceMgr(const std::string& reg_address_list, const std::string& local_ip,
        uint16_t local_port, const std::string& report_ip, const std::string& report_port);
    bool get_service_address(std::size_t balance_index, const std::string& service_name,
        ServiceType type, std::string& ip, std::string& port);
    void stop();
    void get_related_forward_ports(const std::string& agentid, std::vector<uint16_t> &ports);
private:
    typedef std::pair<std::string, std::string> HostPairT;

    bool registerToServiceDiscovery();
    //bool parseServiceNodeInfo(const rapidjson::Value& v, std::string& host);
    //bool parseServiceInfo(const rapidjson::Value& v, std::string& port,
    //    std::string& service_name, ServiceType& type, std::vector<std::string>& service_tags);
    //bool parseServiceCheckInfo(const rapidjson::Value& v, bool& is_passing);

    //bool parseQueryServicesRsp(const std::string& json_rsp, std::vector<std::string>& services);
    //bool parseServiceInfoRsp(const std::string& json_rsp,
    //    std::vector<std::map<std::string, std::set<HostPairT> > >& node_list);
    bool longPollingRequest(FibpHttpClient& client, const std::string& path,
        const std::string& query,
        uint32_t& indexid, std::string& json_rsp, int timeout = 120);
    void watchServiceNodeChangeFunc(const std::string& name);
    void watchServiceChangeFunc();
    void runFiber();

    void watchPortForwardChangeFunc();

    bool getForwardPortInfo(FibpHttpClient& client, const std::string& port, std::string& port_info,
        std::string& service_name, int& type);
    void watchCurrentClusterName();
    void changeCluster(const std::string& newClusterName);

    typedef std::map<std::string, std::vector<HostPairT> > ServiceHostMapT;
    typedef std::vector<ServiceHostMapT> ServiceHostInfoT;
    ServiceHostInfoT  reg_service_host_info_;
    std::vector<std::pair<std::string, std::string> > reg_address_list_;
    std::string local_ip_;
    uint16_t local_port_;
    std::string report_ip_;
    std::string report_port_;
    bool need_stop_;
    boost::asio::io_service io_service_;
    boost::shared_ptr<boost::thread>  watching_thread_;
    boost::shared_mutex  lock_;
    std::string curClusterName_;
    std::map<std::string, std::set<uint16_t> > ports_used_by_agent_;
};

}

#endif
