#include "ResponseRender.h"
#include <common/Keys.h>
#include <3rdparty/msgpack/msgpack.hpp>
#include <glog/logging.h>

using namespace izenelib::driver;
namespace fibp
{
ResponseRender::ResponseRender(const ServicesRsp& rsp_list)
    : rsp_data_(rsp_list)
{
}

void ResponseRender::generate_port_forward_services_rsp(const std::vector<ForwardInfoT>& infos, izenelib::driver::Value& ret)
{
    for(std::size_t i = 0; i < infos.size(); ++i)
    {
        Value& r = ret();
        r["ServiceName"] = infos[i].service_name;
        r["ServiceType"] = infos[i].service_type;
        r["ForwardPort"] = infos[i].port;
    }
}

void ResponseRender::generate_single_rsp(const std::vector<ServiceCallReq>& req_list,
    std::string& raw_rsp, izenelib::driver::Response& ret)
{
    assert(rsp_data_.size() == 1);
    if (!rsp_data_[0].error.empty())
    {
        ret.addError(rsp_data_[0].error);
        return;
    }
    raw_rsp = rsp_data_[0].rsp;
}

void ResponseRender::generate_rsp(const std::vector<ServiceCallReq>& req_list, izenelib::driver::Value& ret)
{
    int i = 0;
    std::string rsp;
    std::string error;
    for(ServicesRsp::const_iterator it = rsp_data_.begin();
        it != rsp_data_.end(); ++it)
    {
        rsp.clear();
        error = it->error;
        if (req_list[i++].service_type == RPC_Service && !it->rsp.empty())
        {
            const std::string& rspdata = it->rsp;
            // convert rpc response to http json body.
            msgpack::unpacked rspmsg;
            try
            {
                msgpack::unpack(&rspmsg, rspdata.data(), rspdata.size());
                rspmsg.get().convert(&rsp);
            }
            catch(const std::exception& e)
            {
                error = "Convert Service Rpc Response to Json String Failed.";
                error += e.what();
            }
        }
        else
        {
            rsp = it->rsp;
        }

        Value& r = ret();
        r[driver::Keys::service_name] = it->service_name;
        r[driver::Keys::service_rsp] = rsp;
        r[driver::Keys::is_cached] = it->is_cached;
        if (!error.empty())
        {
            r[driver::Keys::service_error] = error;
        }
    }
}

}
