#include "FibpRpcServer.h"
#include <forward-manager/FibpForwardManager.h>
#include <log-manager/FibpLogger.h>
#include <boost/lexical_cast.hpp>
#include <glog/logging.h>
#include <3rdparty/msgpack/msgpack.hpp>

namespace fibp
{

static const std::string method_names[] = {
    "test",
    "call_services_async",
    "call_single_service_async"
};

enum METHOD
{
    METHOD_TEST = 0,
    METHOD_CALL_SERVICES_ASYNC,
    METHOD_CALL_SINGLE_SERVICE_ASYNC,
    COUNT_OF_METHODS
};

struct RpcServicesReq
{
    std::vector<ServiceCallReq> req_list;
    MSGPACK_DEFINE(req_list);
};

struct RpcServicesRsp
{
    std::vector<ServiceCallRsp> rsp_list;
    MSGPACK_DEFINE(rsp_list);
};

class RpcRequestContext: public boost::enable_shared_from_this<RpcRequestContext>
{
public:
    std::vector<ServiceCallReq> req_list_;
    std::vector<ServiceCallRsp> rsp_list_;
    uint64_t id_;
};

static void rpc_callback(msgpack::rpc::request req, boost::shared_ptr<RpcRequestContext> context)
{
    RpcServicesRsp rsp;
    rsp.rsp_list.swap(context->rsp_list_);
    req.result(rsp);
    FibpLogger::get()->endServiceCall(context->id_);
}

static void rpc_single_service_callback(msgpack::rpc::request req, boost::shared_ptr<RpcRequestContext> context)
{
    RpcServicesRsp rsp;
    rsp.rsp_list.swap(context->rsp_list_);
    if (rsp.rsp_list.size() != 1)
    {
        req.error(std::string("SERVER_RETURN_ERROR"));
    }
    else
    {
        if (rsp.rsp_list[0].error.empty())
        {
            msgpack::object rsp_obj;
            msgpack::unpacked unpacked_data;
            msgpack::unpack(&unpacked_data, rsp.rsp_list[0].rsp.data(),
                rsp.rsp_list[0].rsp.size());
            rsp_obj = unpacked_data.get();
            req.result(rsp_obj);
        }
        else
        {
            req.error(rsp.rsp_list[0].error);
        }
    }
    FibpLogger::get()->endServiceCall(context->id_);
}


FibpRpcServer::FibpRpcServer(uint16_t port, uint32_t threadnum)
    : port_(boost::lexical_cast<uint16_t>(port)),
    threadnum_(threadnum), fibp_forward_mgr_(NULL)
{
}

FibpRpcServer::~FibpRpcServer()
{
    stop();
}

void FibpRpcServer::start()
{
    fibp_forward_mgr_ = FibpForwardManager::get();
    instance.listen("0.0.0.0", port_);
    instance.start(threadnum_);
    LOG(INFO) << "FibpRpcServer listening on :" << port_;
}

void FibpRpcServer::stop()
{
    instance.end();
    instance.join();
}

void FibpRpcServer::call_single_service_async(msgpack::rpc::request req, boost::shared_ptr<RpcRequestContext> context)
{
    fibp_forward_mgr_->call_services_in_fiber(context->id_, context->req_list_,
        context->rsp_list_, boost::bind(&rpc_single_service_callback, req, context));
}

void FibpRpcServer::call_services_async(msgpack::rpc::request req, boost::shared_ptr<RpcRequestContext> context)
{
    fibp_forward_mgr_->call_services_in_fiber(context->id_, context->req_list_,
        context->rsp_list_, boost::bind(&rpc_callback, req, context));
}

void FibpRpcServer::dispatch(msgpack::rpc::request req)
{
    try
    {
        std::string method;
        req.method().convert(&method);
        if (method == method_names[METHOD_TEST])
        {
            req.result(true);
        }
        else if (method == method_names[METHOD_CALL_SERVICES_ASYNC])
        {
            uint64_t id = FibpLogger::get()->startServiceCall(__FUNCTION__);
            msgpack::type::tuple<RpcServicesReq> params;
            req.params().convert(&params);
            RpcServicesReq& rpc_req = params.get<0>();
            boost::shared_ptr<RpcRequestContext> context(new RpcRequestContext);
            context->id_ = id;
            rpc_req.req_list.swap(context->req_list_);
            call_services_async(req, context);
        }
        else if (method.find(method_names[METHOD_CALL_SINGLE_SERVICE_ASYNC]) == 0)
        {
            std::string server_method = method.substr(method_names[METHOD_CALL_SINGLE_SERVICE_ASYNC].length() + 1);
            std::size_t split_pos = server_method.find("/");
            if (split_pos == std::string::npos)
            {
                req.error(std::string("ARGUMENT_ERROR"));
                return;
            }
            RpcServicesReq rpc_req;
            rpc_req.req_list.resize(1);
            rpc_req.req_list.back().service_name = server_method.substr(0, split_pos);
            rpc_req.req_list.back().service_api = server_method.substr(split_pos + 1);
            msgpack::sbuffer tmp_buf;
            msgpack::packer<msgpack::sbuffer> pk(tmp_buf);
            pk.pack(req.params());
            rpc_req.req_list.back().service_req_data.assign(tmp_buf.data(), tmp_buf.size());
            rpc_req.req_list.back().service_cluster = "dev";
            rpc_req.req_list.back().service_type = RPC_Service;
            uint64_t id = FibpLogger::get()->startServiceCall(__FUNCTION__);
            boost::shared_ptr<RpcRequestContext> context(new RpcRequestContext);
            context->id_ = id;
            rpc_req.req_list.swap(context->req_list_);
            call_single_service_async(req, context);
        }
        else
        {
            req.error(std::string("NO_METHOD_ERROR"));
        }
    }
    catch(const msgpack::type_error& e)
    {
        req.error(std::string("ARGUMENT_ERROR"));
        LOG(WARNING) << "msgpack call type error.";
    }
    catch(const std::exception& e)
    {
        req.error(std::string(e.what()));
        LOG(WARNING) << "rpc server got exception: " << e.what();
    }
}

}
