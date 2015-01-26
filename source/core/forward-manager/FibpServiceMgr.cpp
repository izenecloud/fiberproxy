#include "FibpServiceMgr.h"
#include "FibpClient.h"
#include "FibpForwardManager.h"
#include <log-manager/FibpLogger.h>
#include <fiber-server/HttpProtocolHandler.h>
#include <fiber-server/yield.hpp>
#include <fiber-server/loop.hpp>
#include <fiber-server/spawn.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/fiber/all.hpp>
#include <glog/logging.h>
#include <3rdparty/rapidjson/reader.h>
#include <3rdparty/rapidjson/document.h>
#include <util/driver/writers/JsonWriter.h>

namespace rj = rapidjson;
namespace fibp
{

static const std::size_t AGENT_ID_LEN = 10;
static const std::string connector("-");
static const std::string http_service_str("http");
static const std::string rpc_service_str("rpc");
static const std::string raw_service_str("raw");
static const std::string dev_cluster_str("dev");
typedef std::pair<std::string, std::string> HostPairT;

static bool parseQueryServicesRsp(const std::string& json_rsp, std::vector<std::string>& services)
{
    services.clear();
    rj::Document doc;
    bool err = doc.Parse<0>(json_rsp.c_str()).HasParseError();
    if (err)
    {
        LOG(INFO) << "parsing failed.";
        return false;
    }
    if (!doc.IsObject())
    {
        LOG(INFO) << "services response is not a object." << json_rsp;
        return false;
    }
    for(rj::Value::ConstMemberIterator itr = doc.MemberBegin();
        itr != doc.MemberEnd(); ++itr)
    {
        std::string name = itr->name.GetString();
        services.push_back(name);
    }
    return true;
}

static bool parseServiceNodeInfo(const rj::Value& v, std::string& host)
{
    if (!v.IsObject())
        return false;
    for(rj::Value::ConstMemberIterator itr = v.MemberBegin();
        itr != v.MemberEnd(); ++itr)
    {
        std::string key = itr->name.GetString();

        if (key == "Address")
        {
            host = itr->value.GetString();
            return true;
        }
    }
    return false;
}

static bool parseServiceInfo(const rj::Value& v, std::string& port,
    std::string& service_name, ServiceType& type, std::vector<std::string>& service_tags)
{
    if (!v.IsObject())
        return false;
    type = Custom_Service;
    service_tags.clear();
    service_name.clear();
    for(rj::Value::ConstMemberIterator itr = v.MemberBegin();
        itr != v.MemberEnd(); ++itr)
    {
        const std::string key = itr->name.GetString();
        if (key == "Port")
        {
            port = boost::lexical_cast<std::string>(itr->value.GetInt());
        }
        else if (key == "Tags")
        {
            const rj::Value& tag_list = itr->value;
            if (!tag_list.IsArray())
            {
            }
            else
            {
                for(rj::SizeType i = 0; i < tag_list.Size(); ++i)
                {
                    std::string tag = tag_list[i].GetString();
                    boost::algorithm::to_lower(tag);
                    if (tag == http_service_str)
                    {
                        type = HTTP_Service;
                    }
                    else if (tag == raw_service_str)
                    {
                        type = Raw_Service;
                    }
                    else if (tag == rpc_service_str)
                    {
                        type = RPC_Service;
                    }
                    else
                    {
                        service_tags.push_back(tag);
                    }
                }
            }
        }
        else if (key == "Service")
        {
            service_name = itr->value.GetString();
        }
    }
    if (!service_name.empty() && service_tags.empty())
    {
        service_tags.push_back(dev_cluster_str);
    }
    return true;
}

static bool parseServiceCheckInfo(const rj::Value& v, bool& is_passing)
{
    is_passing = true;
    if (!v.IsArray())
    {
        return true;
    }
    for(rj::SizeType i = 0; i < v.Size(); ++i)
    {
        const rj::Value& check = v[i];
        for(rj::Value::ConstMemberIterator it = check.MemberBegin();
            it != check.MemberEnd(); ++it)
        {
            if (it->name.GetString() == std::string("Status"))
            {
                is_passing = (it->value.GetString() == std::string("passing"));
                if (!is_passing)
                {
                    LOG(INFO) << "service check not passing." << it->value.GetString();
                    return true;
                }
            }
        }
    }
    return true;
}

static bool parseServiceInfoRsp(const std::string& json_rsp,
    std::vector<std::map<std::string, std::set<HostPairT> > >& node_list)
{
    rj::Document doc;
    bool err = doc.Parse<0>(json_rsp.c_str()).HasParseError();
    if (err)
    {
        LOG(INFO) << "parsing failed." << json_rsp;
        return false;
    }
    if (!doc.IsArray())
    {
        LOG(INFO) << "response is not a list of nodes." << json_rsp;
        return false;
    }

    node_list.resize(End_Service);
    for(std::size_t i = 0; i < doc.Size(); ++i)
    {
        const rj::Value& node = doc[i];
        std::string host, port, service_name;
        ServiceType type = Custom_Service;
        std::vector<std::string> service_tags;
        bool is_passing = false;
        bool ret = true;
        for(rj::Value::ConstMemberIterator itr = node.MemberBegin();
            itr != node.MemberEnd(); ++itr)
        {
            std::string key = itr->name.GetString();

            if (key == "Node")
            {
                ret = parseServiceNodeInfo(itr->value, host);
                if (!ret)
                {
                    LOG(INFO) << "parse node info failed.";
                    break;
                }
            }
            else if (key == "Service")
            {
                ret = parseServiceInfo(itr->value, port, service_name, type, service_tags);
                if (!ret)
                {
                    LOG(INFO) << "parse service info failed.";
                    break;
                }
            }
            else if (key == "Checks")
            {
                ret = parseServiceCheckInfo(itr->value, is_passing);
                if (!ret)
                {
                    LOG(INFO) << "parse check info failed.";
                    break;
                }
            }
        }
        if (!is_passing || !ret)
        {
            LOG(INFO) << "ignore failed node: " << host << ":" << port << ", " << service_name;
            continue;
        }
        std::map<std::string, std::set<HostPairT> >& server_info = node_list[type];
        for(std::size_t i = 0; i < service_tags.size(); ++i)
        {
            server_info[service_name + connector + service_tags[i]].insert(std::make_pair(host, port));
        }
    }
    return true;
}


static bool parseForwardPortListRsp(const std::string& json_rsp,
    std::vector<std::string>& forward_service_list)
{
    rj::Document doc;
    bool err = doc.Parse<0>(json_rsp.c_str()).HasParseError();
    if (err)
    {
        LOG(INFO) << "parsing failed." << json_rsp;
        return false;
    }
    if (!doc.IsArray())
    {
        LOG(INFO) << "response is not a list of ports." << json_rsp;
        return false;
    }
    const static std::string forward_prefix("fibp-forward-port/");
    forward_service_list.reserve(doc.Size());
    for(rj::SizeType i = 0; i < doc.Size(); ++i)
    {
        const rj::Value& servicekey = doc[i];
        std::string tmpstr = servicekey.GetString();
        if (tmpstr.length() > forward_prefix.length())
            forward_service_list.push_back(tmpstr.substr(forward_prefix.length()));
    }
    LOG(INFO) << "forward service number: " << forward_service_list.size();
    return true;
}

void reportServiceStats(boost::asio::io_service& io_service, std::string report_ip, std::string report_port, bool& need_stop)
{
    FibpLogger::ServiceStatMapT serviceStats;
    FibpHttpClient client(io_service, report_ip, report_port);
    while(!need_stop)
    {
        boost::this_fiber::interruption_point();
        http::request_t http_request;
        http_request.method_ = http::POST;
        http_request.path_ = "/api/monitor/cluster/report-service";
        http_request.keep_alive_ = true;

        izenelib::driver::Value reqValue;
        FibpLogger::get()->getRecentServiceStats(serviceStats);
        int cnt = 0;
        for(FibpLogger::ServiceStatMapT::iterator it = serviceStats.begin();
            it != serviceStats.end(); ++it)
        {
            for(std::size_t i = 0; i < it->second.size(); ++i)
            {
                cnt++;
                izenelib::driver::Value& stat_value = reqValue();
                stat_value["Name"] = it->first;
                stat_value["Latency"] = it->second[i].avgLatency;
                stat_value["RequestPerSec"] = it->second[i].requestPerSec;
                stat_value["Timestamp"] = it->second[i].timestamp;
            }
        }
        if (cnt == 0)
        {
            LOG(INFO) << "no stats for any service.";
        }
        else
        {
            izenelib::driver::JsonWriter writer;
            writer.write(reqValue, http_request.body_);

            //LOG(INFO) << http_request.body_;
            int timeout_ms = 1000;

            bool ret = client.send_http_request(http_request, timeout_ms);
            boost::this_fiber::interruption_point();
            if (!ret)
            {
                LOG(WARNING) << "report request failed.";
            }
            else
            {
                http::response_t http_rsp;
                ret = client.get_http_response(http_rsp);
                if (!ret)
                {
                    LOG(WARNING) << "get report response failed." << http_rsp.status_message_;
                }
            }
        }
        boost::this_fiber::interruption_point();
        boost::this_fiber::sleep_for(boost::chrono::seconds(10));
    }
}

FibpServiceMgr::FibpServiceMgr(const std::string& reg_address_list, const std::string& local_ip,
    uint16_t local_port, const std::string& report_ip, const std::string& report_port)
    : local_ip_(local_ip), local_port_(local_port), report_ip_(report_ip), report_port_(report_port), need_stop_(false)
{
    reg_service_host_info_.resize(End_Service);

    curClusterName_ = dev_cluster_str;
    std::vector<std::string> addr_list;
    boost::split(addr_list, reg_address_list, boost::is_any_of(","),
        boost::token_compress_on);
    for(std::size_t i = 0; i < addr_list.size(); ++i)
    {
        std::vector<std::string> ip_port;
        boost::split(ip_port, addr_list[i], boost::is_any_of(":"));
        if (ip_port.size() != 2)
            continue;
        reg_address_list_.push_back(std::make_pair(ip_port[0], ip_port[1]));
        LOG(INFO) << "dns server added :" << ip_port[0] << ":" << ip_port[1];
    }
    if (reg_address_list_.empty())
    {
        LOG(ERROR) << "No Dns server!";
        return;
    }
    watching_thread_.reset(new boost::thread(boost::bind(&FibpServiceMgr::runFiber, this)));
}

void FibpServiceMgr::stop()
{
    need_stop_ = true;
    io_service_.stop();
    if (watching_thread_)
        watching_thread_->join();
}

void FibpServiceMgr::runFiber()
{
    //boost::fibers::asio::spawn(io_service_,
    //    boost::bind(&FibpServiceMgr::watchServiceChangeFunc, this));

    boost::fibers::fiber(boost::bind(&FibpServiceMgr::watchServiceChangeFunc, this)).detach();
    //boost::fibers::asio::spawn(io_service_,
    //    boost::bind(&FibpServiceMgr::watchPortForwardChangeFunc, this));

    boost::fibers::fiber(boost::bind(&FibpServiceMgr::watchPortForwardChangeFunc, this)).detach();
    boost::fibers::fiber(boost::bind(&FibpServiceMgr::watchCurrentClusterName, this)).detach();

    boost::fibers::fiber(boost::bind(&reportServiceStats, boost::ref(io_service_),
            report_ip_, report_port_, boost::ref(need_stop_))).detach();

    //boost::fibers::asio::spawn(io_service_,
    //    boost::bind(&reportServiceStats, boost::ref(io_service_),
    //        report_ip_, report_port_, boost::ref(need_stop_)));


    boost::fibers::fiber f(boost::bind(boost::fibers::asio::run_service, boost::ref(io_service_)));
    f.join();
}

void FibpServiceMgr::watchCurrentClusterName()
{
    FibpHttpClient client(io_service_, report_ip_, report_port_);
    std::string indexid("0");
    while(!need_stop_)
    {
        boost::this_fiber::interruption_point();
        http::request_t http_request;
        http_request.method_ = http::GET;
        http_request.path_ = "/api/local/get-cluster";
        http_request.keep_alive_ = true;
        http_request.query_ = "index=" + indexid + "&wait=600s";

        izenelib::driver::Value reqValue;

        bool ret = client.send_http_request(http_request, 0);
        boost::this_fiber::interruption_point();
        if (!ret)
        {
            LOG(WARNING) << "report request failed.";
        }
        else
        {
            http::response_t http_rsp;
            ret = client.get_http_response(http_rsp);
            if (!ret)
            {
                LOG(WARNING) << "get report response failed." << http_rsp.status_message_;
            }
            else
            {
                std::string json_rsp = http_rsp.body_;
                rj::Document doc;
                bool err = doc.Parse<0>(json_rsp.c_str()).HasParseError();
                if (err)
                {
                    LOG(INFO) << "parsing cluster name failed.";
                    ret = false;
                }
                else if (!doc.IsObject())
                {
                    LOG(INFO) << "cluster name response is not a object." << json_rsp;
                    ret = false;
                }
                else
                {
                    std::string newClusterName = doc.GetString();

                    for(rj::Value::ConstMemberIterator itr = doc.MemberBegin();
                        itr != doc.MemberEnd(); ++itr)
                    {
                        std::string name = itr->name.GetString();
                        if (name == "Name")
                        {
                            newClusterName = itr->value.GetString();
                            break;
                        }
                    }
                    indexid = http::find_header(http_rsp.headers_, "X-Consul-Index");
                    if (indexid == "")
                    {
                        indexid = "0";
                    }
                    LOG(WARNING) << "forward cluster changed to: " << newClusterName;
                    if (newClusterName != curClusterName_)
                    {
                        changeCluster(newClusterName);
                    }
                }
            }
        }

        boost::this_fiber::interruption_point();
        if (!ret)
            boost::this_fiber::sleep_for(boost::chrono::seconds(10));
    }
}

void FibpServiceMgr::changeCluster(const std::string& newClusterName)
{
    if (newClusterName.empty())
    {
        LOG(INFO) << "empty cluster name.";
        return;
    }
    curClusterName_ = newClusterName;
}

bool FibpServiceMgr::longPollingRequest(FibpHttpClient& client, const std::string& path,
    const std::string& query,
    uint32_t& indexid, std::string& json_rsp, int timeout)
{
    http::request_t http_request;
    http_request.method_ = http::GET;
    http_request.path_ = path;
    http_request.query_ = query;
    if (!query.empty())
    {
        http_request.query_ += "&";
    }
    if (timeout >= 0)
    {
        http_request.query_ += "index=" + boost::lexical_cast<std::string>(indexid) +
            "&wait=" + boost::lexical_cast<std::string>(timeout) + "s";
    }
    http_request.keep_alive_ = true;

    int timeout_ms = 0;
    bool ret = client.send_http_request(http_request, timeout_ms);
    if (!ret)
    {
        LOG(WARNING) << "send long polling request failed.";
        return ret;
    }
    http::response_t http_rsp;
    ret = client.get_http_response(http_rsp);
    if (!ret)
    {
        LOG(WARNING) << "get long polling response failed." << http_rsp.status_message_;
        return ret;
    }
    json_rsp = http_rsp.body_;
    indexid = boost::lexical_cast<uint32_t>(http::find_header(http_rsp.headers_, "X-Consul-Index"));
    return true;
}

void FibpServiceMgr::watchServiceNodeChangeFunc(const std::string& name)
{
    uint32_t indexid = 0;
    std::string query_path = "/v1/health/service/" + name;
    uint32_t balance_index = 0;
    std::set<std::string> last_service_names;

    const std::pair<std::string, std::string>& ip_port = reg_address_list_[0];
    boost::shared_ptr<FibpHttpClient> client;
    client.reset(new FibpHttpClient(io_service_, ip_port.first, ip_port.second));
    while(!need_stop_)
    {
        std::string json_rsp;
        bool ret = longPollingRequest(*client, query_path, "", indexid, json_rsp);
        if (!ret)
        {
            boost::this_fiber::sleep_for(boost::chrono::seconds(1));
            ++balance_index;
            const std::pair<std::string, std::string>& ip_port = reg_address_list_[balance_index % reg_address_list_.size()];
            client.reset(new FibpHttpClient(io_service_, ip_port.first, ip_port.second));
            continue;
        }
        //LOG(INFO) << "long polling for service returned: " << query_path;
        typedef std::map<std::string, std::set<HostPairT> > MapT;
        std::vector<MapT> node_list;
        ret = parseServiceInfoRsp(json_rsp, node_list);
        if (!ret)
        {
            LOG(INFO) << "parse service node list failed." << name;
            continue;
        }
        for(std::size_t i = 0; i < node_list.size(); ++i)
        {
            boost::unique_lock<boost::shared_mutex> guard(lock_);
            for (std::set<std::string>::iterator lastit = last_service_names.begin();
                lastit != last_service_names.end(); ++lastit)
            {
                reg_service_host_info_[i].erase(*lastit);
            }
            for(MapT::const_iterator it = node_list[i].begin();
                it != node_list[i].end(); ++it)
            {
                std::vector<HostPairT>& host_list = reg_service_host_info_[i][it->first];
                LOG(INFO) << "service added: " << i << ", " << it->first << ", number:" << it->second.size();
                host_list.clear();
                host_list.insert(host_list.end(), it->second.begin(), it->second.end());
                last_service_names.insert(it->first);
            }
        }
    }
}

void FibpServiceMgr::watchServiceChangeFunc()
{
    //registerToServiceDiscovery();

    std::set<std::string> watching_services;
    uint32_t indexid = 0;
    uint32_t balance_index = 0;
    const std::pair<std::string, std::string>& ip_port = reg_address_list_[0];
    boost::shared_ptr<FibpHttpClient> client;
    client.reset(new FibpHttpClient(io_service_, ip_port.first, ip_port.second));
    while(!need_stop_)
    {
        std::string json_rsp;
        bool ret = longPollingRequest(*client, "/v1/catalog/services", "", indexid, json_rsp);
        if (!ret)
        {
            boost::this_fiber::sleep_for(boost::chrono::seconds(1));
            ++balance_index;

            const std::pair<std::string, std::string>& ip_port = reg_address_list_[balance_index % reg_address_list_.size()];
            client.reset(new FibpHttpClient(io_service_, ip_port.first, ip_port.second));
            continue;
        }
        std::vector<std::string> services;
        ret = parseQueryServicesRsp(json_rsp, services);
        for(std::size_t i = 0; i < services.size(); ++i)
        {
            if (watching_services.find(services[i]) != watching_services.end())
                continue;
            boost::fibers::fiber f(boost::bind(&FibpServiceMgr::watchServiceNodeChangeFunc,
                    this, services[i]));
            f.detach();
            watching_services.insert(services[i]);
        }
    }
}

void FibpServiceMgr::get_related_forward_ports(const std::string& agentid, std::vector<uint16_t>& port_list)
{
    boost::shared_lock<boost::shared_mutex> guard(lock_);
    std::map<std::string, std::set<uint16_t> >::const_iterator it = ports_used_by_agent_.find(agentid);
    if (it == ports_used_by_agent_.end())
        return;
    std::set<uint16_t>::const_iterator pit = it->second.begin();
    for(; pit != it->second.end(); ++pit)
    {
        port_list.push_back(*pit);
    }
}

struct ServiceInfo
{
    std::string service_name;
    int type;
};

void FibpServiceMgr::watchPortForwardChangeFunc()
{
    uint32_t indexid = 0;
    uint32_t balance_index = 0;
    // the (service-port, the agent ids that using the service)
    std::map<uint16_t, std::set<std::string> > last_ports;
    std::map<std::string, uint16_t> service_port_map;
    std::map<std::string, ServiceInfo> previous_keys;
    const std::pair<std::string, std::string>& ip_port = reg_address_list_[0];
    boost::shared_ptr<FibpHttpClient> client;
    client.reset(new FibpHttpClient(io_service_, ip_port.first, ip_port.second));
    while(!need_stop_)
    {
        std::string json_rsp;
        bool ret = longPollingRequest(*client, "/v1/kv/fibp-forward-port", "keys", indexid, json_rsp);
        if (!ret)
        {
            boost::this_fiber::sleep_for(boost::chrono::seconds(1));
            ++balance_index;

            const std::pair<std::string, std::string>& ip_port = reg_address_list_[balance_index % reg_address_list_.size()];
            client.reset(new FibpHttpClient(io_service_, ip_port.first, ip_port.second));
            continue;
        }
        std::vector<std::string> current_forward_services;
        // the key is agentid-servicename, data is (servicename, servicetype)
        ret = parseForwardPortListRsp(json_rsp, current_forward_services);
        if (!ret)
        {
            LOG(INFO) << "parse the forward services data failed." << json_rsp;
            continue;
        }
        if (current_forward_services.size() == 0)
        {
            LOG(INFO) << "no forward services available. current used port: " << last_ports.size();
            previous_keys.clear();
            continue;
        }
        
        for (std::map<uint16_t, std::set<std::string> >::iterator it = last_ports.begin();
            it != last_ports.end(); ++it)
        {
            it->second.clear();
        }

        std::map<std::string, ServiceInfo> new_keys;
        for(std::size_t i = 0; i < current_forward_services.size(); ++i)
        {
            const std::string& forwardkey = current_forward_services[i];
            if (forwardkey.length() <= AGENT_ID_LEN)
            {
                LOG(INFO) << "forward key invalid: " << forwardkey;
                continue;
            }
            LOG(INFO) << "handle : " << forwardkey;
            std::string agentid = forwardkey.substr(0, AGENT_ID_LEN);
            std::string service_uniquestr;

            std::string info;
            ServiceInfo si;
            std::map<std::string, ServiceInfo>::const_iterator keyit = previous_keys.find(forwardkey);
            if (keyit != previous_keys.end())
            {
                // old key
                si = keyit->second;
            }
            else
            {
                ret = getForwardPortInfo(*client, forwardkey, info, si.service_name, si.type);
                if (!ret)
                {
                    LOG(INFO) << "get forward info failed for : " << forwardkey;
                    continue;
                }
            }
            service_uniquestr = si.service_name + boost::lexical_cast<std::string>(si.type);

            new_keys[forwardkey] = si;
            std::map<std::string, uint16_t>::const_iterator s_it = service_port_map.find(service_uniquestr);
            if (s_it != service_port_map.end())
            {
                LOG(INFO) << "forward port already exist for service: " << service_uniquestr;
                last_ports[s_it->second].insert(agentid);
                //FibpForwardManager::get()->updateForwardService(s_it->second, service_name, type);
                continue;
            }

            uint16_t createdPort = 0;
            ret = FibpForwardManager::get()->startPortForward(createdPort, si.service_name, si.type);
            if (!ret)
            {
                LOG(WARNING) << "start port forward failed for service : " << service_uniquestr;
                continue;
            }
            
            service_port_map[service_uniquestr] = createdPort;
            last_ports[createdPort].insert(agentid);
        }
        previous_keys = new_keys;
        std::set<uint16_t> removed_ports;
        for(std::map<uint16_t, std::set<std::string> >::iterator it = last_ports.begin();
            it != last_ports.end(); ++it)
        {
            if (it->second.size() == 0)
            {
                ForwardInfoT info;
                if (FibpForwardManager::get()->getForwardService(it->first, info))
                {
                    std::string service_uniquestr = info.service_name + boost::lexical_cast<std::string>(info.service_type);
                    service_port_map.erase(service_uniquestr);
                }
                FibpForwardManager::get()->stopPortForward(it->first);
                removed_ports.insert(it->first);
            }
        }
        for(std::set<uint16_t>::const_iterator it = removed_ports.begin();
            it != removed_ports.end(); ++it)
        {
            last_ports.erase(*it);
            LOG(INFO) << "forward port removed: " << *it;
        }

        boost::unique_lock<boost::shared_mutex> guard(lock_);
        for (std::map<uint16_t, std::set<std::string> >::const_iterator it = last_ports.begin();
            it != last_ports.end(); ++it)
        {
            std::set<std::string>::const_iterator it2 = it->second.begin();
            for(; it2 != it->second.end(); ++it2)
            {
                ports_used_by_agent_[*it2].insert(it->first);
            }
        }
    }
}

bool FibpServiceMgr::getForwardPortInfo(FibpHttpClient& client, const std::string& forwardkey, std::string& info,
    std::string& service_name, int& type)
{
    uint32_t indexid = 0;
    bool ret = longPollingRequest(client, "/v1/kv/fibp-forward-port/" + forwardkey, "raw", indexid, info, -1);
    if (!ret)
    {
        return false;
    }
    std::vector<std::string> elems;
    std::vector<std::string> addr_list;
    boost::split(elems, info, boost::is_any_of(","),
        boost::token_compress_on);
    if (elems.size() < 2)
    {
        LOG(WARNING) << "forward service info not valid: " << info;
        return false;
    }
    service_name = elems[0];
    boost::algorithm::to_lower(elems[1]);
    if (elems[1] == http_service_str)
    {
        type = HTTP_Service;
    }
    else if (elems[1] == rpc_service_str)
    {
        type = RPC_Service;
    }
    else if (elems[1] == raw_service_str)
    {
        type = Raw_Service;
    }
    else
    {
        type = Custom_Service;
    }
    return true;
}

bool FibpServiceMgr::get_service_address(std::size_t balance_index,
    const std::string& service_name,
    ServiceType type,
    std::string& ip, std::string& port)
{
    if (type >= reg_service_host_info_.size())
    {
        LOG(ERROR) << "service type error: " << type;
        return false;
    }
    std::string service_key = service_name + connector + curClusterName_;

    boost::shared_lock<boost::shared_mutex> guard(lock_);
    ServiceHostMapT::const_iterator host_it = reg_service_host_info_[type].find(service_key);
    if (host_it == reg_service_host_info_[type].end() ||
        host_it->second.empty())
    {
        LOG(INFO) << "service not found : " << service_key;
        return false;
    }
    const HostPairT& host_info = host_it->second[balance_index % host_it->second.size()];
    ip = host_info.first;
    port = host_info.second;
    return true;
}

}
