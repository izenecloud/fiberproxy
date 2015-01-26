#ifndef DISTRIBUTED_CONFIG_H_
#define DISTRIBUTED_CONFIG_H_

#include <sstream>

namespace fibp
{

class DistributedCommonConfig
{
public:
    std::string toString()
    {
        std::stringstream ss;
        ss << "[DistributedCommonConfig]" << std::endl
           << "username: " << userName_ << std::endl
           << "localhost: " << localHost_ << std::endl
           << "BA Port: " << baPort_ << std::endl
           << "worker server port: " << workerPort_ << std::endl
           << "master server port: " << masterPort_  << std::endl;
        return ss.str();
    }

public:
    std::string userName_;
    std::string localHost_;
    unsigned int baPort_;
    unsigned int workerPort_;
    unsigned int masterPort_;
};

}

#endif /* DISTRIBUTED_CONFIG_H_ */
