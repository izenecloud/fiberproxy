#ifndef RPC_SERVER_CONNECTION_CONFIG_H_
#define RPC_SERVER_CONNECTION_CONFIG_H_

#include <string>

namespace fibp
{

struct RpcServerConnectionConfig
{
public:
    std::string host;
    unsigned int rpcPort;
    unsigned int rpcThreadNum;
    unsigned int driverPort;

    RpcServerConnectionConfig(
        const std::string& host_addr = "localhost",
        unsigned int rpc_port = 0,
        unsigned int rpc_thread_num = 10,
        unsigned int driver_port = 0
    )
        : host(host_addr)
        , rpcPort(rpc_port)
        , rpcThreadNum(rpc_thread_num)
        , driverPort(driver_port)
    {}
};

} // namespace fibp

#endif //RPC_SERVER_CONNECTION_CONFIG_H_
