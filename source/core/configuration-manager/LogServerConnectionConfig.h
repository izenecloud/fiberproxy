#ifndef LOG_SERVER_CONNECTION_CONFIG_H_
#define LOG_SERVER_CONNECTION_CONFIG_H_

#include <string>

namespace fibp
{

struct LogServerConnectionConfig
{
public:
    std::string host;
    std::string port;
    std::string log_service;
    std::string log_tag;

    LogServerConnectionConfig(
        const std::string& host_addr = "localhost",
        const std::string host_port = "")
        : host(host_addr)
        , port(host_port)
    {}
};

} // namespace

#endif //LOG_SERVER_CONNECTION_CONFIG_H_
