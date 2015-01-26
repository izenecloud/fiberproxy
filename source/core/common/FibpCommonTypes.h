#ifndef FIBP_COMMON_TYPES_H
#define FIBP_COMMON_TYPES_H

#include <util/izene_serialization.h>
#include <3rdparty/msgpack/msgpack.hpp>
#include <string>
#include <vector>

namespace fibp
{

enum method
{
    DELETE,
    GET,
    HEAD,
    POST,
    PUT
};

enum ServiceType
{
    HTTP_Service,
    RPC_Service,
    Raw_Service,
    Custom_Service,
    End_Service
};

struct ServiceCallReq
{
    std::string service_name;
    std::string service_api;
    int method;
    std::string service_req_data;
    std::string service_cluster;
    int service_type;
    bool enable_cache;
    ServiceCallReq()
        :method(POST), service_type(HTTP_Service), enable_cache(false)
    {
    }
    inline bool operator==(const ServiceCallReq& other) const
    {
        return service_name == other.service_name &&
            service_api == other.service_api &&
            service_req_data == other.service_req_data &&
            service_cluster == other.service_cluster &&
            service_type == other.service_type &&
            method == other.method;
    }

    MSGPACK_DEFINE(service_name, service_api, method, service_req_data, service_cluster,
        service_type, enable_cache);

    DATA_IO_LOAD_SAVE(ServiceCallReq, & service_name & service_api & method
        & service_req_data & service_cluster & service_type);
};

struct ServiceCallRsp
{
    std::string service_name;
    std::string rsp;
    std::string error;
    bool is_cached;
    std::string host;
    std::string port;
    ServiceCallRsp()
        :is_cached(false)
    {
    }
    void swap(ServiceCallRsp& other)
    {
        service_name.swap(other.service_name);
        rsp.swap(other.rsp);
        error.swap(other.error);
        std::swap(is_cached, other.is_cached);
        host.swap(other.host);
        port.swap(other.port);
    }
    MSGPACK_DEFINE(service_name, rsp, error, is_cached, host, port);
    //DATA_IO_LOAD_SAVE(ServiceCallRsp, & service_name & rsp & error & is_cached);
};

typedef std::vector<ServiceCallRsp> ServicesRsp;

struct ForwardInfoT
{
    std::string service_name;
    int service_type;
    uint16_t port;
};


}

MAKE_FEBIRD_SERIALIZATION(fibp::ServiceCallReq)
//MAKE_FEBIRD_SERIALIZATION(fibp::ServiceCallRsp)

#endif
