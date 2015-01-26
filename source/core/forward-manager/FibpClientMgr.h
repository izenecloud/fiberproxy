#ifndef FIBP_CLIENT_MGR_H
#define FIBP_CLIENT_MGR_H

#include "FibpClient.h"
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <common/FibpCommonTypes.h>
#include <vector>
#include <deque>
#include <map>

namespace fibp
{

class FibpClientMgr
{
public:
    FibpClientMgr();
    ~FibpClientMgr();

    FibpClientFuturePtr send_request(
        const std::string& path,
        const std::string& ip, const std::string& port,
        ServiceType calltype, const std::string& reqdata, int to_ms);

    FibpHttpClientPtr send_request(
        const std::string& path,
        http::method method,
        const std::string& ip, const std::string& port,
        const std::string& reqdata, int to_ms);

    FibpClientFuturePtr send_request(
        boost::asio::io_service& io,
        const std::string& path,
        const std::string& ip, const std::string& port,
        ServiceType calltype, const std::string& reqdata, int to_ms);

    FibpHttpClientPtr send_request(
        boost::asio::io_service& io,
        const std::string& path,
        http::method method,
        const std::string& ip, const std::string& port,
        const std::string& reqdata, int to_ms);

    bool get_response(FibpHttpClientPtr client, std::string& rsp, bool& can_retry);
    bool get_response(FibpClientFuturePtr f, std::string& rsp, bool& can_retry);

    boost::asio::io_service& get_io_service()
    {
        return io_service_;
    }

private:
    static void run_service(boost::asio::io_service& io_service);
    typedef std::map<std::string, std::deque<FibpHttpClientPtr> > HttpClientPoolT;
    typedef std::map<std::string, FibpClientBasePtr> ClientPoolT;
    HttpClientPoolT  http_client_pool_list_;
    std::vector<ClientPoolT>  client_pool_list_;
    std::map<std::string, uint32_t>  client_num_;
    boost::shared_ptr<boost::thread> running_thread_;
    boost::asio::io_service io_service_;
};

}

#endif
