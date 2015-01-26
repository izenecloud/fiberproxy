#include "FibpLogger.h"
#include <forward-manager/FibpServiceMgr.h>
#include <fiber-server/loop.hpp>
#include <forward-manager/FibpClient.h>
#include <3rdparty/folly/RWSpinLock.h>
#include <boost/thread.hpp>
#include <boost/atomic.hpp>
#include <sstream>
#include <util/hashFunction.h>
#include <glog/logging.h>
#include <util/driver/Value.h>
#include <util/driver/writers/JsonWriter.h>
#include <boost/fiber/all.hpp>
#include <3rdparty/rapidjson/document.h>
#include <3rdparty/rapidjson/writer.h>
#include <3rdparty/rapidjson/stringbuffer.h>

#define MAX_LOG_NUM 1000000
#define MAX_LOG_SEND_NUM 1000

namespace rj = rapidjson;
namespace fibp
{

inline void get_nowtime(uint64_t& now_t)
{
    timeval now;
    gettimeofday(&now, 0);
    now_t = now.tv_sec * 1000000 +
        now.tv_usec;
}

FibpLogger::FibpLogger()
    : auto_inc_id_(0), need_stop_(false), fibp_service_mgr_(NULL)
{
    logged_call_list_ = new LogData[MAX_LOG_NUM];
}

void FibpLogger::init(const std::string& log_ip, const std::string& log_port,
    const std::string& log_service)
{
    log_service_ = log_service;
    log_client_.reset(new FibpHttpClient(io_service_, log_ip, log_port));
    running_thread_.reset(new boost::thread(boost::bind(&FibpLogger::runSendLogFiber, this)));
    LOG(INFO) << "log sending to server: " << log_ip << ":" << log_port;
}

void FibpLogger::setServiceMgr(FibpServiceMgr* service_mgr)
{
    fibp_service_mgr_ = service_mgr;
}

FibpLogger::~FibpLogger()
{
    need_stop_ = true;
    io_service_.stop();
    if (running_thread_)
    {
        running_thread_->interrupt();
        running_thread_->join();
    }
    delete[] logged_call_list_;
}

void FibpLogger::findLogService()
{
    if (!fibp_service_mgr_)
        return;
    std::string ip;
    std::string port;
    if (fibp_service_mgr_->get_service_address(0, log_service_, HTTP_Service, ip, port))
    {
        if (log_client_ && log_client_->host() == ip && log_client_->port() == port)
        {
            // no change
            return;
        }
        log_client_.reset(new FibpHttpClient(io_service_, ip, port));
        LOG(INFO) << "Log service was found at : " << ip << ":" << port;
    }
    else
    {
        LOG(INFO) << "Log service not found.";
    }
}

void FibpLogger::runSendLogFiber()
{
    boost::fibers::fiber f(boost::bind(boost::fibers::asio::run_service, boost::ref(io_service_)));
    boost::fibers::fiber f_send(boost::bind(&FibpLogger::sendLogToDB, this));
    f.join();
    f_send.join();
}

// send to influxdb directly.
void FibpLogger::sendLogToDB()
{
    // note: we assumed the send thread will never fall behind too much,
    // so the log writer will never override the unsended items.
    uint64_t next_send = 1;

    rj::StringBuffer strbuf;

    http::request_t req;
    req.method_ = http::POST;
    req.path_ = "/db/FIBP/series?u=root&p=root&time_precision=u";
    req.keep_alive_ = true;
    req.headers_.push_back(std::make_pair("Content-Type", "application/json"));

    ServiceStatMapT serviceStats;
    std::map<std::string, ServiceStat> curTimeStats;
    while(true)
    {
        try
        {
            if (need_stop_)
                break;
            boost::this_thread::interruption_point();
            boost::this_fiber::interruption_point();
            rj::Document document;
            rj::Document::AllocatorType& alloc = document.GetAllocator();
            rj::Value log_item_list(rj::kArrayType);
            log_item_list.Reserve(MAX_LOG_SEND_NUM, alloc);
            std::string tmp;
            rj::Value tmpStr;
            int cnt = 0;

            LogData* logdata = &(logged_call_list_[next_send % MAX_LOG_NUM]);
            rj::Value api_points(rj::kArrayType);
            rj::Value log_item(rj::kObjectType);
            log_item.AddMember("name", "fibp_api_log", alloc);

            rj::Value col_list(rj::kArrayType);
            col_list.PushBack("time", alloc);
            col_list.PushBack("logid", alloc);
            col_list.PushBack("start_time", alloc);
            col_list.PushBack("end_time", alloc);
            col_list.PushBack("latency", alloc);
            log_item.AddMember("columns", col_list, alloc);

            rj::Value service_log_item(rj::kObjectType);
            rj::Value service_api_points(rj::kArrayType);
            service_log_item.AddMember("name", "fibp_services_log", alloc);
            rj::Value service_col_list(rj::kArrayType);
            service_col_list.PushBack("time", alloc);
            service_col_list.PushBack("logid", alloc);
            service_col_list.PushBack("start_time", alloc);
            service_col_list.PushBack("end_time", alloc);
            service_col_list.PushBack("latency", alloc);
            service_col_list.PushBack("service_name", alloc);
            service_col_list.PushBack("host_port", alloc);
            service_col_list.PushBack("is_fail", alloc);
            service_col_list.PushBack("failed_msg", alloc);
            service_log_item.AddMember("columns", service_col_list, alloc);

            while(logdata->wait_send.load(boost::memory_order_acquire) &&
                cnt < MAX_LOG_SEND_NUM)
            {
                if (auto_inc_id_ - next_send < MAX_LOG_NUM/2)
                {
                    rj::Value single_point(rj::kArrayType);
                    single_point.PushBack(logdata->start_time, alloc);
                    single_point.PushBack(logdata->id, alloc);
                    single_point.PushBack(logdata->start_time, alloc);
                    single_point.PushBack(logdata->end_time, alloc);
                    single_point.PushBack(logdata->end_time - logdata->start_time, alloc);

                    api_points.PushBack(single_point, alloc);

                    for(auto it = logdata->service_send_reply_times.begin();
                        it != logdata->service_send_reply_times.end(); ++it)
                    {
                        for(auto vit = it->second.begin();
                            vit != it->second.end(); ++vit)
                        {
                            rj::Value service_log(rj::kArrayType);
                            service_log.PushBack(vit->start_time, alloc);
                            service_log.PushBack(logdata->id, alloc);
                            service_log.PushBack(vit->start_time, alloc);
                            service_log.PushBack(vit->end_time, alloc);
                            service_log.PushBack(vit->end_time - vit->start_time, alloc);
                            tmpStr.SetString(it->first.c_str(), it->first.size(), alloc);
                            service_log.PushBack(tmpStr, alloc);
                            tmpStr.SetString(vit->host_port.c_str(), vit->host_port.size(), alloc);
                            service_log.PushBack(tmpStr, alloc);
                            tmp = vit->failed_msg;
                            if (!tmp.empty())
                            {
                                service_log.PushBack(true, alloc);
                                tmpStr.SetString(tmp.c_str(), tmp.size(), alloc);
                                service_log.PushBack(tmpStr, alloc);
                            }
                            else
                            {
                                service_log.PushBack(false, alloc);
                                service_log.PushBack("", alloc);
                            }
                            service_api_points.PushBack(service_log, alloc);
                            auto stat_it = curTimeStats.find(it->first);
                            ServiceStat emptyStat = ServiceStat();
                            if (stat_it == curTimeStats.end())
                            {
                                auto p = std::make_pair(it->first, emptyStat);
                                stat_it = curTimeStats.insert(p).first;
                                stat_it->second.timestamp = vit->start_time/1000000; 
                            }
                            if (vit->start_time / 1000000 != stat_it->second.timestamp)
                            {
                                stat_it->second.avgLatency /= stat_it->second.requestPerSec;
                                serviceStats[it->first].push_back(stat_it->second);
                                //LOG(INFO) << "new timestamp for service: " << it->first << ", total samples: " << serviceStats[it->first].size();
                                if (stat_it->second.timestamp - serviceStats[it->first].front().timestamp > 10)
                                {
                                    {
                                        boost::unique_lock<boost::shared_mutex> guard(stat_mutex_);
                                        serviceStats.swap(recentServiceStats_);
                                        //LOG(INFO) << "swapped for service stats to get ready send.";
                                    }
                                    for(ServiceStatMapT::iterator tmpit = serviceStats.begin();
                                        tmpit != serviceStats.end(); ++tmpit)
                                    {
                                        tmpit->second.clear();
                                    }
                                }
                                stat_it->second.reset();
                                stat_it->second.timestamp = vit->start_time / 1000000;
                            }
                            stat_it->second.requestPerSec++;
                            stat_it->second.avgLatency += vit->end_time - vit->start_time;
                        }
                    }

                    ++cnt;
                }
                else
                {
                    if (next_send % MAX_LOG_SEND_NUM == 0)
                    {
                        LOG(INFO) << "log send fall behind too much, abandon some logs." <<
                            next_send << " VS " << auto_inc_id_;
                    }
                }
                logdata->reset();
                ++next_send;
                logdata = &(logged_call_list_[next_send % MAX_LOG_NUM]);
            }
            if (cnt == 0)
            {
                boost::this_fiber::sleep_for(boost::chrono::seconds(1));
                continue;
            }
            LOG(INFO) << "sending : " << api_points.Size() << ", " << service_api_points.Size();
            log_item.AddMember("points", api_points, alloc);
            service_log_item.AddMember("points", service_api_points, alloc);
            log_item_list.PushBack(log_item, alloc);
            log_item_list.PushBack(service_log_item, alloc);

            strbuf.Clear();
            rj::Writer<rj::StringBuffer> writer(strbuf);
            log_item_list.Accept(writer);
            
            req.body_.assign(strbuf.GetString(), strbuf.Size());

            bool ret = log_client_->send_http_request(req, 300);
            if (!ret)
            {
                LOG(INFO) << "Send log failed.";
                findLogService();
            }
            else
            {
                req.body_.clear();
                ret = log_client_->get_response(req.body_);
                LOG(INFO) << req.body_;
            }
            if (cnt < 100)
            {
                boost::this_fiber::sleep_for(boost::chrono::milliseconds(10));
            }
        }
        catch(const boost::thread_interrupted& e)
        {
            break;
        }
        catch(const boost::fibers::fiber_interrupted& e)
        {
            break;
        }
        catch(const std::exception& e)
        {
            LOG(ERROR) << "exception in log sending thread: " << e.what();
        }
    }
    LOG(INFO) << "sending log thread exited.";

}

// send to storm.
void FibpLogger::sendLog()
{
    // note: we assumed the send thread will never fall behind too much,
    // so the log writer will never override the unsended items.
    uint64_t next_send = 1;
    uint32_t send_id = 0;

    rj::StringBuffer strbuf;

    http::request_t req;
    req.method_ = http::POST;
    req.path_ = "/rest/StormServer/postfibp";
    req.keep_alive_ = true;
    req.headers_.push_back(std::make_pair("Content-Type", "application/json"));

    while(true)
    {
        try
        {
            if (need_stop_)
                break;
            boost::this_thread::interruption_point();
            boost::this_fiber::interruption_point();
            rj::Document document;
            rj::Document::AllocatorType& alloc = document.GetAllocator();
            rj::Value log_item_list(rj::kArrayType);
            log_item_list.Reserve(MAX_LOG_SEND_NUM, alloc);
            std::string tmp;
            rj::Value tmpStr;
            int cnt = 0;

            LogData* logdata = &(logged_call_list_[next_send % MAX_LOG_NUM]);
            while(logdata->wait_send.load(boost::memory_order_acquire) &&
                cnt < MAX_LOG_SEND_NUM)
            {
                if (auto_inc_id_ - next_send < MAX_LOG_NUM/10)
                {
                    rj::Value log_item(rj::kObjectType);
                    log_item.AddMember("logid", logdata->id, alloc);
                    log_item.AddMember("start_time", logdata->start_time, alloc);
                    log_item.AddMember("end_time", logdata->end_time, alloc);
                    log_item.AddMember("latency", logdata->end_time - logdata->start_time, alloc);

                    rj::Value service_item_list(rj::kArrayType);
                    for(auto it = logdata->service_send_reply_times.begin();
                        it != logdata->service_send_reply_times.end(); ++it)
                    {
                        for(auto vit = it->second.begin();
                            vit != it->second.end(); ++vit)
                        {
                            rj::Value service_log(rj::kObjectType);
                            tmpStr.SetString(it->first.c_str(), it->first.size(), alloc);
                            service_log.AddMember("name", tmpStr, alloc);
                            service_log.AddMember("start_time", vit->start_time, alloc);
                            service_log.AddMember("end_time", vit->end_time, alloc);
                            tmpStr.SetString(vit->host_port.c_str(), vit->host_port.size(), alloc);
                            service_log.AddMember("host_port", tmpStr, alloc);
                            service_log.AddMember("latency", vit->end_time - vit->start_time, alloc);
                            tmp = vit->failed_msg;
                            if (!tmp.empty())
                            {
                                tmpStr.SetString(tmp.c_str(), tmp.size(), alloc);
                                service_log.AddMember("fail_msg", tmpStr, alloc);
                                service_log.AddMember("is_fail", true, alloc);
                            }
                            else
                            {
                                service_log.AddMember("is_fail", false, alloc);
                            }
                            service_item_list.PushBack(service_log, alloc);
                        }
                    }
                    log_item.AddMember("services", service_item_list, alloc);
                    log_item_list.PushBack(log_item, alloc);

                    ++cnt;
                }
                else
                {
                    if (next_send % MAX_LOG_SEND_NUM == 0)
                    {
                        LOG(INFO) << "log send fall behind too much, abandon some logs." <<
                            next_send << " VS " << auto_inc_id_;
                    }
                }
                logdata->reset();
                ++next_send;
                logdata = &(logged_call_list_[next_send % MAX_LOG_NUM]);
            }
            if (cnt == 0)
            {
                boost::this_fiber::sleep_for(boost::chrono::seconds(1));
                continue;
            }
            LOG(INFO) << "sending : " << log_item_list.Size();

            strbuf.Clear();
            rj::Writer<rj::StringBuffer> writer(strbuf);
            log_item_list.Accept(writer);
            
            rj::Value fibpbean;
            fibpbean.SetObject();
            tmp = boost::lexical_cast<std::string>(++send_id);
            tmpStr.SetString(tmp.c_str(), tmp.size(), alloc);
            fibpbean.AddMember("id", tmpStr, alloc);
            tmpStr.SetString(strbuf.GetString(), strbuf.Size(), alloc);
            fibpbean.AddMember("jsonMessage", tmpStr, alloc);
            document.SetObject();
            document.AddMember("FibpBean", fibpbean, alloc);

            strbuf.Clear();
            rj::Writer<rj::StringBuffer> writer2(strbuf);
            document.Accept(writer2);
            req.body_.assign(strbuf.GetString(), strbuf.Size());

            bool ret = log_client_->send_http_request(req, 300);
            if (!ret)
            {
                LOG(INFO) << "Send log failed.";
            }
            else
            {
                req.body_.clear();
                ret = log_client_->get_response(req.body_);
                LOG(INFO) << req.body_;
            }
            if (cnt < 100)
            {
                boost::this_fiber::sleep_for(boost::chrono::milliseconds(10));
            }
        }
        catch(const boost::thread_interrupted& e)
        {
            break;
        }
        catch(const boost::fibers::fiber_interrupted& e)
        {
            break;
        }
        catch(const std::exception& e)
        {
            LOG(ERROR) << "exception in log sending thread: " << e.what();
        }
    }
    LOG(INFO) << "sending log thread exited.";
}

void FibpLogger::getRecentServiceStats(ServiceStatMapT& recentStats)
{
    boost::shared_lock<boost::shared_mutex> guard(stat_mutex_);
    recentStats = recentServiceStats_;
}

uint64_t FibpLogger::startServiceCall(const std::string& desp)
{
    //uint64_t oldid = auto_inc_id_;
    //while(oldid >= MAX_LOG_NUM - 1 && !auto_inc_id_.compare_exchange_weak(oldid, 0))
    //{
    //}

    uint64_t myid = ++auto_inc_id_;
    //LOG(INFO) << "start for : " << myid;
    LogData& l = logged_call_list_[myid % MAX_LOG_NUM];
    if (l.wait_send || l.id != 0)
    {
        LOG(ERROR) << "reused the wait sending log !!!! " << l.id;
        return 0;
    }
    get_nowtime(l.start_time);
    l.id = myid;
    return myid;
}

void FibpLogger::endServiceCall(uint64_t id)
{
    if (id == 0)
        return;
    //LOG(INFO) << "end for : " << id;
    LogData& l = logged_call_list_[id % MAX_LOG_NUM];
    get_nowtime(l.end_time);
    l.wait_send = true;
}

void FibpLogger::sendServiceRequest(uint64_t id, const std::string& name, const std::string& host,
    const std::string& port)
{
    if (id == 0)
        return;
    //LOG(INFO) << "send " << name << " for " << id;
    LogData& l = logged_call_list_[id % MAX_LOG_NUM];
    l.service_send_reply_times[name].push_back(RequestData());
    get_nowtime(l.service_send_reply_times[name].back().start_time);
    l.service_send_reply_times[name].back().host_port = host + ":" + port;
}

void FibpLogger::getServiceRsp(uint64_t id, const std::string& name)
{
    if (id == 0)
        return;
    //LOG(INFO) << "reply " << name << " for " << id;
    LogData& l = logged_call_list_[id % MAX_LOG_NUM];
    get_nowtime(l.service_send_reply_times[name].back().end_time);
}

void FibpLogger::markTime(uint64_t id, const std::string& file, int line)
{
    //LOG(INFO) << "marked " << file << ":" << line << " for " << id;
}

void FibpLogger::logServiceFailed(uint64_t id, const std::string& name, const std::string& failed_msg)
{
    if (id == 0)
        return;
    LOG(INFO) << "failed : " << id << ", " << name << ", " << failed_msg;
    LogData& l = logged_call_list_[id % MAX_LOG_NUM];
    l.service_send_reply_times[name].back().failed_msg = failed_msg;
}

void FibpLogger::exceedClientLimit(const std::string& dest)
{
    LOG(WARNING) << "connections to the " << dest << " exceed the limit.";
}

void FibpLogger::logCurrentConnections(const std::string& cid, std::size_t num)
{
    if (num > 3)
    {
        LOG(INFO) << "client " << cid << " created connections : " << num;
    }
}

void FibpLogger::logTransaction(bool success, const std::string& api, const std::string& action,
    const std::string& tran_id, const std::string& failed_msg)
{
    LOG(INFO) << "transaction message: " << success << ", " << api << "-" << action << ", " <<
        tran_id << ", " << failed_msg;
}
}
