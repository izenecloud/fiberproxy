#ifndef DISTRIBUTED_UTIL_CONFIG_H_
#define DISTRIBUTED_UTIL_CONFIG_H_

#include <sstream>

namespace fibp
{

struct ZooKeeperConfig
{
    ZooKeeperConfig() : disabled_(false) {}

    bool disabled_;
    std::string zkHosts_;
    unsigned int zkRecvTimeout_;

    std::string toString()
    {
        std::stringstream ss;
        ss << "[ZooKeeper] " << (disabled_ ? "disabled " : "")
           << "hosts: " << zkHosts_ << " timeout: " << zkRecvTimeout_ << std::endl;
        return ss.str();
    }
};

struct DFSConfig
{
    std::string type_;
    bool isSupportFuse_;
    std::string mountDir_;
    std::string server_;
    unsigned int port_;

    DFSConfig()
    : isSupportFuse_(true), port_(0)
    {

    }

    std::string toString()
    {
        std::stringstream ss;
        ss << "========= DistributedUtilConfig ========"<<std::endl;
        ss << "[DFS Config] "<<type_
                << (isSupportFuse_?", support fuse" : ", not support fuse")
                << ", mount on "<<mountDir_
                << ", server="<<server_<<":"<<port_<<std::endl;
        ss << "========================================"<<std::endl;
        return ss.str();
    }
};

struct ServiceDiscoveryConfig
{
    std::string servers_;
};

class DistributedUtilConfig
{
public:
    std::string toString()
    {
        return zkConfig_.toString() + dfsConfig_.toString();
    }

public:
    ZooKeeperConfig zkConfig_;
    DFSConfig dfsConfig_;
    ServiceDiscoveryConfig dns_config_;
};

}

#endif /* DISTRIBUTEDUTILCONFIG_H_ */
