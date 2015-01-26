#include "RequestParser.h"
#include <common/Keys.h>
#include <glog/logging.h>
#include <3rdparty/msgpack/msgpack.hpp>
#include <util/driver/writers/JsonWriter.h>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>

using namespace izenelib::driver;
namespace fibp
{

RequestParser::RequestParser(const izenelib::driver::Value& postdata)
    : client_req_(postdata)
{
}

bool RequestParser::parse_list_forward_service(std::string& agentid)
{
    if (client_req_.type() != Value::kObjectType)
    {
        return false;
    }
    if (client_req_.hasKey("ID"))
    {
        agentid = asString(client_req_["ID"]);
    }
    return true;
}

inline int GetReqMethod(const std::string& method)
{
    int method_int = POST;
    if (method == "DELETE") {
        method_int = DELETE;
    }
    else if (method == "GET")
    {
        method_int = GET;
    }
    else if (method == "PUT") 
    {
        method_int = PUT;
    }
    else if (method == "HEAD")
    {
        method_int = HEAD;
    }
    else
    {
        method_int = POST;
    }

    return method_int;
}

bool RequestParser::parse_single_api_request(int method,
    const std::string& raw_req,
    const std::string& path, std::vector<ServiceCallReq> &req_api_list)
{
    req_api_list.resize(1);

    std::vector<std::string> elems;
    std::string temp = path;
    boost::trim_if(temp, boost::is_any_of("/"));
    boost::split(elems, temp, boost::is_any_of("/"), boost::token_compress_on);
    if (elems.size() < 4)
    {
        return false;
    }
    std::size_t api_pos = 0;
    api_pos += elems.at(0).size() + 1;
    api_pos += elems.at(1).size() + 1;
    req_api_list[0].service_name = elems.at(2);
    api_pos += elems.at(2).size();
    //req_api_list[0].service_cluster = elems.at(3);
    //api_pos += elems.at(3).size();
    req_api_list[0].service_api = temp.substr(api_pos);
    LOG(INFO) << "single api : " << req_api_list[0].service_api << ", method: " << method;
    req_api_list[0].service_req_data = raw_req;
    req_api_list[0].service_type = 0;

    req_api_list[0].method = method;
    return true;
}

bool RequestParser::parse_api_request(std::vector<ServiceCallReq> &req_api_list)
{
    const Value& api_list = client_req_;
    if (api_list.type() != Value::kArrayType)
    {
        return false;
    }
    req_api_list.resize(api_list.size());
    try
    {
        for(std::size_t i = 0; i < req_api_list.size(); ++i)
        {
            const Value& api_data = api_list(i);
            req_api_list[i].service_name = asString(api_data[driver::Keys::service_name]);
            req_api_list[i].service_api = asString(api_data[driver::Keys::service_api]);
            req_api_list[i].method = GetReqMethod(asString(api_data[driver::Keys::service_method]));
            req_api_list[i].service_req_data = asString(api_data[driver::Keys::service_req_data]);
            req_api_list[i].service_cluster = asString(api_data[driver::Keys::service_cluster]);
            req_api_list[i].service_type = (ServiceType)asInt(api_data[driver::Keys::service_type]);
            if (req_api_list[i].service_type == RPC_Service)
            {
                // convert the json body to rpc body.
                msgpack::type::tuple<std::string> rpc_body(req_api_list[i].service_req_data);
                msgpack::sbuffer buf;
                msgpack::pack(buf, rpc_body);
                req_api_list[i].service_req_data.assign(buf.data(), buf.size()); 
            }
            req_api_list[i].enable_cache = asBool(api_data[driver::Keys::enable_cache]);
        }
    }
    catch(const std::exception& e)
    {
        LOG(ERROR) << "parse request data failed." << e.what();
        req_api_list.clear();
        return false;
    }
    return true;
}

}

