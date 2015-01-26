#ifndef FIBP_RESPONSE_RENDER_H
#define FIBP_RESPONSE_RENDER_H

#include <util/driver/Value.h>
#include <util/driver/Response.h>
#include <common/FibpCommonTypes.h>

namespace fibp
{

class ResponseRender
{
public:
    ResponseRender(const ServicesRsp& rsp_list);
    void generate_single_rsp(const std::vector<ServiceCallReq>& req_list,
        std::string& raw_rsp, izenelib::driver::Response& ret);
    void generate_rsp(const std::vector<ServiceCallReq>& req_list, izenelib::driver::Value& ret);

    static void generate_port_forward_services_rsp(const std::vector<ForwardInfoT>& infos, izenelib::driver::Value& ret);
private:
    const ServicesRsp& rsp_data_;
};

}
#endif
