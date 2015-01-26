#ifndef FIBP_SERVICE_CACHE_H
#define FIBP_SERVICE_CACHE_H

#include <common/FibpCommonTypes.h>
#include <cache/concurrent_cache.hpp>

namespace fibp
{

class FibpServiceCache
{
public:
    typedef ServiceCallReq key_type;
    typedef ServiceCallRsp value_type;
    typedef izenelib::concurrent_cache::ConcurrentCache<key_type, value_type> cache_type;
    explicit FibpServiceCache(uint32_t cache_size);
    bool get(const ServiceCallReq& req, ServiceCallRsp& rsp);
    void set(const ServiceCallReq& req, const ServiceCallRsp& rsp);
    void clear();
private:
    cache_type cache_;
};

}

#endif
