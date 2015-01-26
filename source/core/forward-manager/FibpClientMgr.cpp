#include "FibpClientMgr.h"
#include "FibpClientFuture.h"
#include <log-manager/FibpLogger.h>
#include <glog/logging.h>
#include <boost/fiber/all.hpp>
#include <fiber-server/loop.hpp>

#define MAX_CLIENT_NUM  100

namespace fibp
{

inline std::string getClientId(const std::string& host, const std::string& port)
{
    static std::string delim(":");
    return host + delim + port;
}

FibpClientMgr::FibpClientMgr()
{
    client_pool_list_.resize(End_Service);
    running_thread_.reset(new boost::thread(boost::bind(&FibpClientMgr::run_service, boost::ref(io_service_))));
}

FibpClientMgr::~FibpClientMgr()
{
    io_service_.stop();
    running_thread_->join();
}

void FibpClientMgr::run_service(boost::asio::io_service& io_service)
{
    boost::fibers::fiber f(boost::bind(boost::fibers::asio::run_service, boost::ref(io_service)));
    f.join();
}
FibpHttpClientPtr FibpClientMgr::send_request(
    const std::string& path,
    http::method method,
    const std::string& ip, const std::string& port,
    const std::string& reqdata, int to_ms)
{
    return send_request(io_service_, path, method, ip, port, reqdata, to_ms);
}
 
FibpHttpClientPtr FibpClientMgr::send_request(
    boost::asio::io_service& io,
    const std::string& path,
    http::method method,
    const std::string& ip, const std::string& port,
    const std::string& reqdata, int to_ms)
{
    FibpHttpClientPtr client;
    HttpClientPoolT& client_pool = http_client_pool_list_;
    std::string client_id = getClientId(ip, port);
    HttpClientPoolT::iterator it = client_pool.find(client_id);
    if (it != client_pool.end() && !it->second.empty())
    {
        client = it->second.front();
        it->second.pop_front();
    }
    else
    {
        if (client_num_[client_id] > MAX_CLIENT_NUM)
        {
            FibpLogger::get()->exceedClientLimit(client_id);
            return FibpHttpClientPtr();
        }
        client.reset(new FibpHttpClient(io, ip, port));
        client_num_[client_id]++;
        FibpLogger::get()->logCurrentConnections(client_id, client_num_[client_id]);
    }
    bool ret = client->send_request(path, method, reqdata, to_ms);
    if (!ret)
    {
        client_pool[client_id].push_back(client);
        return FibpHttpClientPtr();
    }
    return client;
}

FibpClientFuturePtr FibpClientMgr::send_request(
    const std::string& path,
    const std::string& ip, const std::string& port,
    ServiceType calltype, const std::string& reqdata, int to_ms)
{
    return send_request(io_service_, path, ip, port, calltype, reqdata, to_ms);
}

FibpClientFuturePtr FibpClientMgr::send_request(
    boost::asio::io_service& io,
    const std::string& path,
    const std::string& ip, const std::string& port,
    ServiceType calltype, const std::string& reqdata, int to_ms)
{
    assert(calltype < client_pool_list_.size());
    if (calltype >= client_pool_list_.size())
    {
        LOG(ERROR) << "error call type: " << calltype;
        throw -1;
    } 
    if (calltype == HTTP_Service)
    {
        LOG(ERROR) << "http not allowed here.";
        throw -1;
    }
    FibpClientBasePtr client;
    FibpClientFuturePtr future; 
    ClientPoolT& client_pool = client_pool_list_[calltype];
    std::string client_id = getClientId(ip, port);
    ClientPoolT::iterator it = client_pool.find(client_id);
    if (it != client_pool.end() && it->second)
    {
        client = it->second;
    }
    else
    {
        switch(calltype)
        {
        case RPC_Service:
            client.reset(new FibpRpcClient(io, ip, port));
            break;
        case Raw_Service:
            client.reset(new FibpRawClient(io, ip, port));
            break;
        default:
            LOG(ERROR) << "No supported service type." << calltype;
            return future;
            break;
        }
        client_num_[client_id]++;
        FibpLogger::get()->logCurrentConnections(client_id, client_num_[client_id]);
        client_pool[client_id] = client;
    }
    future = client->send_request(path, reqdata, to_ms);
    return future;
}

bool FibpClientMgr::get_response(FibpHttpClientPtr client, std::string& rsp, bool& can_retry)
{
    if (!client)
        return false;
    std::string client_id = getClientId(client->host(), client->port());
    bool ret = client->get_response(rsp);
    can_retry = client->can_retry();
    http_client_pool_list_[client_id].push_back(client);
    return ret;
}

bool FibpClientMgr::get_response(FibpClientFuturePtr f, std::string& rsp, bool& can_retry)
{
    if (!f)
        return false;
    bool ret = f->getRsp(rsp, can_retry);
    return ret;
}

}
