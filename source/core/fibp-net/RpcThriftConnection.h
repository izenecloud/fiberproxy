#ifndef _RPC_THRIFT_CONNECTION_H_
#define _RPC_THRIFT_CONNECTION_H_

#include "RpcServerConnectionConfig.h"
#include <boost/scoped_ptr.hpp>
#include <util/singleton.h>

namespace fibp
{

class RpcThriftConnection 
{
public:
    RpcThriftConnection();
    ~RpcThriftConnection();

    void packRpcRequest(const std::string& reqdata, std::string& packed_reqdata);
    void unpackRpcResponse(const std::string& rpc_rsp, std::string& unpacked_rsp);
    void test();

private:
    RpcServerConnectionConfig config_;
};

}

#endif /* _RPC_SERVER_CONNECTION_H_ */
