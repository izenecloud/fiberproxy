#include "FibpServiceCache.h"

namespace fibp
{

FibpServiceCache::FibpServiceCache(uint32_t cache_size)
    : cache_(cache_size, izenelib::cache::LRLFU)
{
}

bool FibpServiceCache::get(const ServiceCallReq& req, ServiceCallRsp& rsp)
{
    if (!req.enable_cache)
        return false;
    value_type tmp;
    bool ret = cache_.get(req, tmp); 
    if (ret)
    {
        tmp.swap(rsp);
        rsp.is_cached = true;
    }
    return ret;
}

void FibpServiceCache::set(const ServiceCallReq& req, const ServiceCallRsp& rsp)
{
    if (!rsp.error.empty() || !req.enable_cache)
        return;
    cache_.insert(req, rsp);
}

void FibpServiceCache::clear()
{
    cache_.clear();
}

}
