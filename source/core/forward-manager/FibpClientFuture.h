#ifndef FIBP_CLIENT_FUTURE_H
#define FIBP_CLIENT_FUTURE_H

#include <boost/shared_ptr.hpp>
#include <boost/system/error_code.hpp>
#include <string>
#include <stdint.h>
#include <boost/fiber/condition.hpp>
#include <boost/asio.hpp>
#include <boost/asio/deadline_timer.hpp>

namespace fibp
{

class FibpClientFuture
{
public:
    FibpClientFuture(boost::asio::io_service& io, uint32_t fid = 0, uint32_t timeout = 2);
    virtual ~FibpClientFuture(){}
    void set_result(const std::string& rsp, bool is_success, bool can_retry);
    bool getRsp(std::string& rsp, bool& can_retry);
    uint32_t getid()
    {
        return future_id_;
    }

private:
    uint32_t future_id_;
    uint32_t timeout_;
    std::string rsp_;
    bool is_success_;
    bool can_retry_;
    bool done_;
    boost::fibers::condition_variable cond_;
    boost::fibers::mutex mutex_;
    boost::asio::deadline_timer  deadline_;
};

}

#endif
