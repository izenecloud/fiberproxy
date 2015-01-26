#include "FibpForwardManager.h"
#include "FibpClientMgr.h"
#include "FibpServiceMgr.h"
#include "FibpClientFuture.h"
#include "FibpServiceCache.h"
#include "FibpTransactionMgr.h"
#include "FibpPortForwardMgr.h"
#include <fiber-server/FiberPool.hpp>
#include <glog/logging.h>
#include <boost/lexical_cast.hpp>
#include <util/hashFunction.h>
#include <sstream>
#include <boost/fiber/all.hpp>
#include <fiber-server/spawn.hpp>
#include <log-manager/FibpLogger.h>
#include <3rdparty/msgpack/msgpack.hpp>
#include <cstdlib>
#include <time.h>

namespace fibp
{

FibpForwardManager::FibpForwardManager()
{
}

void FibpForwardManager::init(const std::string& dns_host_list,
    const std::string& local_ip, uint16_t local_port,
    const std::string& report_ip, const std::string& report_port,
    std::size_t thread_size)
{
    std::srand(time(NULL));
    client_mgr_list_.init(thread_size);
    fiber_pool_list_.init(thread_size);

    service_mgr_.reset(new FibpServiceMgr(dns_host_list, local_ip, local_port, report_ip, report_port));
    port_forward_mgr_.reset(new FibpPortForwardMgr(service_mgr_.get()));
    service_cache_.reset(new FibpServiceCache(1000000));
    service_fail_stat_.rehash(10000);
    FibpLogger::get()->setServiceMgr(service_mgr_.get());
}

bool FibpForwardManager::startPortForward(uint16_t &port, const std::string& service_name,
    int type)
{
    bool ret = port_forward_mgr_->startPortForward(port);
    if (ret)
        port_forward_mgr_->updateForwardService(port, service_name, type);
    return ret;
}

void FibpForwardManager::stopPortForward(uint16_t port)
{
    port_forward_mgr_->stopPortForward(port);
}

void FibpForwardManager::getAllPortForwardServices(const std::string& agentid, std::vector<ForwardInfoT> &forward_services)
{
    std::vector<uint16_t> ports;
    service_mgr_->get_related_forward_ports(agentid, ports);
    forward_services.reserve(ports.size());
    for(std::size_t i = 0; i < ports.size(); ++i)
    {
        forward_services.push_back(ForwardInfoT());
        bool ret = port_forward_mgr_->getForwardService(ports[i], forward_services.back());
        if (!ret)
        {
            forward_services.pop_back();
        }
    }
}

bool FibpForwardManager::getForwardService(uint16_t port, ForwardInfoT& info)
{
    return port_forward_mgr_->getForwardService(port, info);
}

void FibpForwardManager::stop()
{
    port_forward_mgr_->stopAll();
    service_mgr_->stop();
    service_cache_->clear();
    client_mgr_list_.clear();
    fiber_pool_list_.clear();
    FibpLogger::get()->setServiceMgr(NULL);
}

void FibpForwardManager::call_services_in_fiber(boost::asio::io_service& io,
    uint64_t id,
    const std::vector<ServiceCallReq>& call_api_list,
    ServicesRsp& rsp_list, callback_t cb, bool do_transaction)
{
    FIBP_THREAD_MARK_LOG(id);
    FibpClientMgr& client_mgr = client_mgr_list_.getThreadObj();

    post_to_fiber_pool(
            io, id, client_mgr, call_api_list,
            rsp_list, cb, do_transaction);
}

void FibpForwardManager::call_services_in_fiber(uint64_t id,
    const std::vector<ServiceCallReq>& call_api_list,
    ServicesRsp& rsp_list, callback_t cb, bool do_transaction)
{
    FIBP_THREAD_MARK_LOG(id);
    FibpClientMgr& client_mgr = client_mgr_list_.getThreadObj();

    client_mgr.get_io_service().post(
        boost::bind(&FibpForwardManager::post_to_fiber_pool, this,
            boost::ref(client_mgr.get_io_service()), id, boost::ref(client_mgr),
            boost::ref(call_api_list), boost::ref(rsp_list), cb, do_transaction));
    //boost::fibers::detail::scheduler::instance()->wakeup();
    FIBP_THREAD_MARK_LOG(id);
}

void FibpForwardManager::call_single_service(boost::asio::io_service& io, uint64_t id,
    FibpClientMgr& client_mgr,
    const ServiceCallReq& req,
    ServiceCallRsp& rsp)
{
    FIBP_THREAD_MARK_LOG(id);
    rsp.service_name = req.service_name;
    static const int MAX_RETRY = 3;
    static const std::string local_test("local_test");
    int retry_counter = 0;
    if (req.service_name == local_test)
    {
        FibpLogger::get()->sendServiceRequest(id, req.service_name, "127.0.0.1", "0");
        rsp.rsp = local_test;
        FibpLogger::get()->getServiceRsp(id, req.service_name);
        return;
    }
    std::size_t balance_index = rand();
    bool is_success = false;
    while(++retry_counter <= MAX_RETRY)
    {
        std::string ip;
        std::string port;
        int timeout_ms = 5000*retry_counter;
        bool ret = service_mgr_->get_service_address(++balance_index, req.service_name,
            (ServiceType)req.service_type, ip, port);
        if (!ret)
        {
            //LOG(INFO) << "service not found: " << req.service_name << " in cluster:" << req.service_cluster;
            rsp.error = "Service Not Found.";
            break;
        }
        FibpLogger::get()->sendServiceRequest(id, req.service_name, ip, port);
        std::string rspdata;
        bool can_retry = true;
        rsp.host = ip;
        rsp.port = port;
        if (req.service_type == HTTP_Service)
        {
            FibpHttpClientPtr f = client_mgr.send_request(io, req.service_api, (http::method)req.method, ip, port,
                req.service_req_data, timeout_ms);
            if (!f)
            {
                FibpLogger::get()->logServiceFailed(id, req.service_name, "Send Data Failed.");
                ++service_fail_stat_[req.service_name];
                if (retry_counter == MAX_RETRY)
                    rsp.error = "Send Service Request Failed. ";
                continue;
            }
            ret = client_mgr.get_response(f, rspdata, can_retry);
            FibpLogger::get()->getServiceRsp(id, req.service_name);
        }
        else
        {
            FibpClientFuturePtr f = client_mgr.send_request(io, req.service_api, ip, port,
                (ServiceType)req.service_type,
                req.service_req_data, timeout_ms);
            if (!f)
            {
                FibpLogger::get()->logServiceFailed(id, req.service_name, "Send Data Failed.");
                ++service_fail_stat_[req.service_name];
                if (retry_counter == MAX_RETRY)
                    rsp.error = "Send Service Request Failed. ";
                continue;
            }
            ret = client_mgr.get_response(f, rspdata, can_retry);
            FibpLogger::get()->getServiceRsp(id, req.service_name);
        }
        if (!ret)
        {
            FibpLogger::get()->logServiceFailed(id, req.service_name, rspdata);
            ++service_fail_stat_[req.service_name];
            if (!can_retry || retry_counter == MAX_RETRY)
            {
                rsp.error = "Get Service Response Failed. " + rspdata;
                break;
            }
        }
        else
        {
            rsp.rsp = rspdata;
            is_success = true;
            break;
        }
    }
    if (is_success)
    {
        service_cache_->set(req, rsp);
    }
    else
    {
        if(service_fail_stat_[req.service_name] % 10 == 0)
        {
            LOG(INFO) << "service get response failed. " << req.service_name <<
                ", total failed: " << service_fail_stat_[req.service_name];
        }
        if (req.enable_cache)
        {
            service_cache_->get(req, rsp);
        }
    }
}
 
void FibpForwardManager::call_single_service(boost::asio::io_service& io, uint64_t id,
    FibpClientMgr& client_mgr,
    const ServiceCallReq& req,
    ServiceCallRsp& rsp,
    int& call_num, boost::fibers::condition_variable& cond)
{
    call_single_service(io, id, client_mgr, req, rsp);
    --call_num;
    if (call_num == 0)
    {
        cond.notify_all();
    }
}

void FibpForwardManager::post_to_fiber_pool(boost::asio::io_service& io,
    uint64_t id,
    FibpClientMgr& client_mgr,
    const std::vector<ServiceCallReq>& call_api_list,
    ServicesRsp& rsp_list, callback_t cb, bool do_transaction)
{
    FiberPool& pool = fiber_pool_list_.getThreadObj();
    pool.schedule_task_from_fiber(
        boost::bind(&FibpForwardManager::call_services_in_fiber, this,
            boost::ref(pool), boost::ref(io), id, boost::ref(client_mgr),
            boost::ref(call_api_list), boost::ref(rsp_list), cb, do_transaction));
}

void FibpForwardManager::call_services_in_fiber(
    FiberPool& pool,
    boost::asio::io_service& io,
    uint64_t id,
    FibpClientMgr& client_mgr,
    const std::vector<ServiceCallReq>& call_api_list,
    ServicesRsp& rsp_list, callback_t cb,
    bool do_transaction)
{
    rsp_list.resize(call_api_list.size());
    bool check = true;
    if (do_transaction)
    {
        // only http allowed to make transaction service.
        for(std::size_t i = 0; i < call_api_list.size(); ++i)
        {
            if (call_api_list[i].service_type != HTTP_Service)
            {
                check = false;
                break;
            }
        }
        if (!check)
        {
            for(std::size_t i = 0; i < rsp_list.size(); ++i)
            {
                rsp_list[i].service_name = call_api_list[i].service_name;
                rsp_list[i].error = "transaction is supported only if all services using http protocol.";
            }
        }
    }
    if (check)
    {
        FIBP_THREAD_MARK_LOG(id);
        // try call all services, any failed service will not affect the others,
        // only success response will be returned (failed message from business server may be returned).
        if (call_api_list.size() == 1)
        {
            const ServiceCallReq& req = call_api_list[0];
            call_single_service(io, id, client_mgr, req, rsp_list[0]);
        }
        else
        {
            boost::fibers::condition_variable cond;
            int call_num = call_api_list.size();
            for(std::size_t i = 0; i < call_api_list.size(); ++i)
            {
                const ServiceCallReq& req = call_api_list[i];
                pool.schedule_task_from_fiber(
                    boost::bind(&FibpForwardManager::call_single_service, this,
                        boost::ref(io), id,
                        boost::ref(client_mgr), boost::ref(req),
                        boost::ref(rsp_list[i]), boost::ref(call_num),
                        boost::ref(cond)));
            }

            boost::fibers::mutex lock;
            boost::unique_lock<boost::fibers::mutex> guard(lock);
            while(call_num > 0)
            {
                cond.wait(guard);
            }
        }
        FIBP_THREAD_MARK_LOG(id);
        if (do_transaction)
        {
            bool need_cancel = false;
            std::vector<std::string> tran_id_list;
            tran_id_list.resize(rsp_list.size());

            for(std::size_t i = 0; i < rsp_list.size(); ++i)
            {
                if (!rsp_list[i].error.empty())
                {
                    need_cancel = true;
                    tran_id_list[i] = transaction_mgr_->get_transaction_id(rsp_list[i].error);
                }
                else
                {
                    tran_id_list[i] = transaction_mgr_->get_transaction_id(rsp_list[i].rsp);
                }
            }
            for(std::size_t i = 0; i < rsp_list.size(); ++i)
            {
                if (need_cancel)
                {
                    if (tran_id_list[i].empty())
                    {
                        continue;
                    }
                    transaction_mgr_->cancel(&client_mgr, rsp_list[i].host, rsp_list[i].port,
                        call_api_list[i].service_api, tran_id_list[i]);
                }
                else
                {
                    transaction_mgr_->confirm(&client_mgr, rsp_list[i].host, rsp_list[i].port,
                        call_api_list[i].service_api, tran_id_list[i]);
                }
            }
        }
    }
    if (cb)
    {
        cb();
    }
}

}

