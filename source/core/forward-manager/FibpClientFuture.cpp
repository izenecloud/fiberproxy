#include "FibpClientFuture.h"
#include "FibpClient.h"
#include <boost/asio.hpp>
#include <boost/fiber/all.hpp>
#include <fiber-server/yield.hpp>
#include <glog/logging.h>
#include <boost/chrono/duration.hpp>

#include <stdint.h>

namespace ba = boost::asio;
namespace bs = boost::system;
namespace fibp
{

static const std::string TIMEOUT_ERR("Server Timed Out.");

FibpClientFuture::FibpClientFuture(boost::asio::io_service& io, uint32_t fid, uint32_t timeout)
    :future_id_(fid), timeout_(timeout), is_success_(false), can_retry_(true), done_(false), deadline_(io)
{
}

void FibpClientFuture::set_result(const std::string& rsp, bool is_success, bool can_retry)
{
    //boost::unique_lock<boost::fibers::mutex> lk(mutex_);
    rsp_ = rsp;
    is_success_ = is_success;
    can_retry_ = can_retry;
    done_ = true;
    deadline_.cancel();
}

bool FibpClientFuture::getRsp(std::string& rsp, bool& can_retry)
{
    if (!done_)
    {
        boost::system::error_code ec;
        deadline_.expires_from_now(boost::posix_time::milliseconds(timeout_));
        deadline_.async_wait(boost::fibers::asio::yield[ec]);
    }
    //LOG(INFO) << "future returned : " << future_id_ << " in fiber: " << boost::this_fiber::get_id();
    if (!done_)
    {
        rsp = TIMEOUT_ERR;
        can_retry = true;
    }
    else
    {
        rsp = rsp_;
        can_retry = can_retry_;
    }
    return is_success_;
}

}
