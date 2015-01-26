#ifndef FIBP_FORWARD_MANAGER_H
#define FIBP_FORWARD_MANAGER_H

#include <common/FibpCommonTypes.h>
#include <common/MultiThreadObjMgr.hpp>
#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/fiber/all.hpp>
#include <boost/asio.hpp>
#include <boost/unordered_map.hpp>
#include <util/singleton.h>
#include <map>
#include <3rdparty/folly/RWSpinLock.h>

namespace fibp
{
class FibpClientMgr;
class FibpServiceMgr;
class FibpServiceCache;
class FibpTransactionMgr;
class FiberPool;
class FibpPortForwardMgr;

class FibpForwardManager
{
public:
    typedef boost::function<void()> callback_t;
    static FibpForwardManager* get()
    {
        return izenelib::util::Singleton<FibpForwardManager>::get();
    }
    FibpForwardManager();
    void init(const std::string& dns_host_list, const std::string& local_ip,
        uint16_t local_port, const std::string& report_ip, const std::string& report_port,
        std::size_t thread_num);

    void call_services_in_fiber(uint64_t id,
        const std::vector<ServiceCallReq>& call_api_list,
        ServicesRsp& rsp_list, callback_t cb, bool do_transaction = false);

    // call direct in the io thread.
    void call_services_in_fiber(boost::asio::io_service& io,
        uint64_t id,
        const std::vector<ServiceCallReq>& call_api_list,
        ServicesRsp& rsp_list, callback_t cb, bool do_transaction = false);

    bool startPortForward(uint16_t &port, const std::string& service_name,
        int type);
    void stopPortForward(uint16_t port);
    void getAllPortForwardServices(const std::string& agentid, std::vector<ForwardInfoT> &forward_services);

    bool getForwardService(uint16_t port, ForwardInfoT& info);
    void stop();

private:

    void post_to_fiber_pool(boost::asio::io_service& io,
        uint64_t id,
        FibpClientMgr& client_mgr,
        const std::vector<ServiceCallReq>& call_api_list,
        ServicesRsp& rsp_list, callback_t cb, bool do_transaction = false);

    void call_services_in_fiber(
        FiberPool& pool,
        boost::asio::io_service& io,
        uint64_t id, FibpClientMgr& client_mgr,
        const std::vector<ServiceCallReq>& call_api_list,
        ServicesRsp& rsp_list, callback_t cb,
        bool do_transaction = false);

    void call_single_service(boost::asio::io_service& io,
        uint64_t id,
        FibpClientMgr& client_mgr,
        const ServiceCallReq& req,
        ServiceCallRsp& rsp);

    void call_single_service(boost::asio::io_service& io,
        uint64_t id,
        FibpClientMgr& client_mgr,
        const ServiceCallReq& req,
        ServiceCallRsp& rsp,
        int& call_num, boost::fibers::condition_variable& cond);

    typedef MultiThreadObjMgr<FibpClientMgr> ClientMgrListT;
    ClientMgrListT client_mgr_list_;
    boost::shared_ptr<FibpServiceMgr> service_mgr_;
    boost::shared_ptr<FibpServiceCache> service_cache_;
    boost::shared_ptr<FibpTransactionMgr> transaction_mgr_;
    typedef MultiThreadObjMgr<FiberPool> FiberPoolListT;
    FiberPoolListT  fiber_pool_list_;
    boost::unordered_map<std::string, uint32_t>  service_fail_stat_;
    boost::shared_ptr<FibpPortForwardMgr> port_forward_mgr_;
};

}
#endif
