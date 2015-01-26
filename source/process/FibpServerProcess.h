#ifndef FIBP_PROCESS_H_
#define FIBP_PROCESS_H_

#include <fiber-server/FiberDriverServerV2.h>
#include <fiber-server/FiberHttpServerV2.h>
#include <fiber-server/FibpRpcServer.h>
#include <common/ProcessOptions.h>

#include <boost/scoped_ptr.hpp>
#include <boost/shared_ptr.hpp>

#include <string>

class FibpProcess
{
public:
    FibpProcess() {}

    int run();

    bool initialize(const std::string& configFileDir, const ProcessOptions& po);

private:
    bool initConnectionServer(const ProcessOptions& po);
    void stopConnectionServer();
    bool initLogManager();

private:
    std::string configDir_;

    boost::shared_ptr<izenelib::driver::Router> driverRouter_;
    boost::scoped_ptr<fibp::FiberDriverServerV2> driverServer_;
    boost::scoped_ptr<fibp::FiberHttpServerV2>  httpServer_;
    boost::scoped_ptr<fibp::FibpRpcServer>  rpcServer_;
};

#endif /*PROCESS_H_*/
