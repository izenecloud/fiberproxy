#ifndef FIBP_LOGGER_H
#define FIBP_LOGGER_H

#include <common/MultiThreadObjMgr.hpp>
#include <map>
#include <string>
#include <vector>
#include <list>
#include <boost/shared_ptr.hpp>
#include <boost/unordered_map.hpp>
#include <boost/asio.hpp>
#include <boost/atomic.hpp>

namespace fibp
{

class FibpHttpClient;
class FibpServiceMgr;
class FibpLogger
{
public:
    static FibpLogger* get()
    {
        return izenelib::util::Singleton<FibpLogger>::get();
    }
    FibpLogger();
    ~FibpLogger();
    void init(const std::string& log_ip, const std::string& log_port,
        const std::string& log_service);
    uint64_t startServiceCall(const std::string& desp);
    void endServiceCall(uint64_t id);
    void sendServiceRequest(uint64_t id, const std::string& name, const std::string& host,
        const std::string& port);
    void getServiceRsp(uint64_t id, const std::string& name);
    void markTime(uint64_t id, const std::string& file, int line);
    void exceedClientLimit(const std::string& dest);
    void logCurrentConnections(const std::string& cid, std::size_t num);
    void logServiceFailed(uint64_t id, const std::string& name, const std::string& failed_msg);
    void logTransaction(bool success, const std::string& api, const std::string& action,
        const std::string& tran_id, const std::string& failed_msg);

    struct ServiceStat
    {
        ServiceStat()
            :avgLatency(0), requestPerSec(0), timestamp(0)
        {
        }
        double avgLatency;
        int requestPerSec;
        uint64_t timestamp;
        void reset()
        {
            avgLatency = 0;
            requestPerSec = 0;
            timestamp = 0;
        }
    };
    typedef std::map<std::string, std::vector<ServiceStat> > ServiceStatMapT;
    void getRecentServiceStats(ServiceStatMapT& recentStats);
    void setServiceMgr(FibpServiceMgr* service_mgr);

private:
    void sendLog();
    void sendLogToDB();
    void runSendLogFiber();
    void findLogService();
    struct RequestData
    {
        RequestData()
            : start_time(0), end_time(0)
        {
        }
        uint64_t start_time;
        uint64_t end_time;
        std::string host_port;
        std::string failed_msg;
    };
    struct LogData
    {
        boost::atomic<bool> wait_send;
        uint64_t id;
        uint64_t start_time;
        uint64_t end_time;
        std::map<std::string, std::vector<RequestData> >  service_send_reply_times;
        //std::map<std::string, std::string>  marked_times;
        //std::map<std::string, std::string>  timeline;
        LogData()
            : wait_send(false), id(0), start_time(0), end_time(0)
        {
        }
        void reset()
        {
            id = 0;
            start_time = 0;
            end_time = 0;
            service_send_reply_times.clear();
            wait_send.store(false, boost::memory_order_release);
        }
    };
    static int const cacheline_size = 64;
    typedef char cacheline_pad_t [cacheline_size];
    cacheline_pad_t pad0_;

    LogData*  logged_call_list_;

    cacheline_pad_t pad1_;
    boost::shared_ptr<FibpHttpClient> log_client_;
    boost::shared_ptr<boost::thread> running_thread_;
    boost::asio::io_service io_service_;

    cacheline_pad_t pad2_;
    boost::atomic<uint64_t> auto_inc_id_;

    cacheline_pad_t pad3_;

    bool need_stop_;
    ServiceStatMapT recentServiceStats_;
    boost::shared_mutex stat_mutex_;
    FibpServiceMgr* fibp_service_mgr_;
    std::string log_service_;
};

//typedef MultiThreadObjMgr<FibpLogger> FibpLoggerMgr;

#define FIBP_THREAD_MARK_LOG(id) FibpLogger::get()->markTime(id, __FILE__, __LINE__)

}

#endif
