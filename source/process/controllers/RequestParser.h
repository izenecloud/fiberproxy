#ifndef FIBP_FORWARD_REQUEST_PARSER_H
#define FIBP_FORWARD_REQUEST_PARSER_H

#include <util/driver/Value.h>
#include <common/FibpCommonTypes.h>

namespace fibp
{
// parser the multi API body in the post data from the client and 
// send them to different business servers.
class RequestParser
{
public:
    RequestParser(const izenelib::driver::Value& postdata);

    bool parse_list_forward_service(std::string& agentid);
    bool parse_single_api_request(int method,
        const std::string& raw_req,
        const std::string& path, std::vector<ServiceCallReq> &req_api_list);
    bool parse_api_request(std::vector<ServiceCallReq> &req_api_list);

private:
    const izenelib::driver::Value& client_req_;
};

}

#endif
