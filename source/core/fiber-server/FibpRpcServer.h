#ifndef FIBP_RPC_SERVER_H
#define FIBP_RPC_SERVER_H

#include <string>
#include <3rdparty/msgpack/rpc/server.h>

namespace fibp
{
class FibpForwardManager;
class RpcRequestContext;
class FibpRpcServer: public msgpack::rpc::server::base
{
public:
    FibpRpcServer(uint16_t port, uint32_t threadnum);
    ~FibpRpcServer();
    void start();
    void stop();

    void call_single_service_async(msgpack::rpc::request req, boost::shared_ptr<RpcRequestContext> context);
    void call_services_async(msgpack::rpc::request req, boost::shared_ptr<RpcRequestContext> context);
    virtual void dispatch(msgpack::rpc::request req);

private:
    uint16_t port_;
    uint32_t threadnum_;
    FibpForwardManager* fibp_forward_mgr_;
};

}

#endif
