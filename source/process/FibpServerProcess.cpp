#include "FibpServerProcess.h"

#include <common/OnSignal.h>
#include <common/XmlConfigParser.h>
#include <common/RouterInitializer.h>
#include <forward-manager/FibpForwardManager.h>
#include <log-manager/FibpLogger.h>

#include <util/ustring/UString.h>
#include <util/driver/IPRestrictor.h>
#include <util/singleton.h>
#include <glog/logging.h>

#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>

#include <iostream>
#include <sstream>
#include <string>
#include <set>
#include <vector>
#include <map>

using namespace fibp;
using namespace boost::filesystem;
using namespace izenelib::util;
using namespace izenelib::driver;

namespace bfs = boost::filesystem;

#define ipRestrictor ::izenelib::driver::IPRestrictor::getInstance()

bool FibpProcess::initialize(const std::string& configFileDir, const ProcessOptions& po)
{
    if( !exists(configFileDir) || !is_directory(configFileDir) ) return false;
    try
    {
        configDir_ = configFileDir;
        boost::filesystem::path p(configFileDir);
        FibpConfig::get()->setHomeDirectory(p.string());
        if( !FibpConfig::get()->parseConfigFile( bfs::path(p/"config.xml").string() ) )
        {
            return false;
        }
    }
    catch ( izenelib::util::ticpp::Exception & e )
    {
        cerr << e.what() << endl;
        return false;
    }

    bool ret = initConnectionServer(po);
    if (!ret)
        return false;
    return initLogManager();
}

bool FibpProcess::initLogManager()
{
    // the log used for monitor, we send all api status to remote log collector.
    const LogServerConnectionConfig logconfig = FibpConfig::get()->getLogServerConfig();
    FibpLogger::get()->init(logconfig.host, logconfig.port, logconfig.log_service);
    return true;
}

bool FibpProcess::initConnectionServer(const ProcessOptions& po)
{
    const BrokerAgentConfig& baConfig = FibpConfig::get()->getBrokerAgentConfig();
    std::size_t threadPoolSize = baConfig.threadNum_;
    bool enableTest = baConfig.enableTest_;
    unsigned int port = baConfig.port_;

    boost::asio::ip::tcp::endpoint endpoint(boost::asio::ip::tcp::v4(),port);

    // init Router
    driverRouter_.reset(new ::izenelib::driver::Router);

    initializeDriverRouter(*driverRouter_, enableTest);

    boost::shared_ptr<FiberDriverConnectionFactory> factory(
        new FiberDriverConnectionFactory(driverRouter_)
    );

    driverServer_.reset(
        new FiberDriverServerV2(endpoint, factory, threadPoolSize)
    );

    boost::asio::ip::tcp::endpoint http_endpoint(boost::asio::ip::tcp::v4(), port + 1);
    boost::shared_ptr<HttpConnectionFactory> http_factory(
        new HttpConnectionFactory(driverRouter_));
    httpServer_.reset(new FiberHttpServerV2(http_endpoint, http_factory, threadPoolSize));

    std::string dns_servers;
    dns_servers = po.getRegistryAddr();
    if (dns_servers.empty())
    {
        dns_servers = FibpConfig::get()->distributedUtilConfig_.dns_config_.servers_;
    }
    std::string report_addr = po.getReportAddr();
    std::size_t port_pos = report_addr.find(":");
    if (port_pos == std::string::npos)
    {
        LOG(ERROR) << "The report address is wrong, should be ip:port. " << report_addr;
        return false;
    }
    std::string report_ip = report_addr.substr(0, port_pos);
    std::string report_port = report_addr.substr(port_pos + 1);
    FibpForwardManager::get()->init(dns_servers,
        FibpConfig::get()->distributedCommonConfig_.localHost_,
        port, report_ip, report_port, threadPoolSize);

    rpcServer_.reset(new FibpRpcServer(
            port + 2, threadPoolSize));
    addExitHook(boost::bind(&FibpProcess::stopConnectionServer, this));

    return true;
}

void FibpProcess::stopConnectionServer()
{
    if (driverServer_)
    {
        driverServer_->stop();
    }
    if (httpServer_)
    {
        httpServer_->stop();
    }
    if (rpcServer_)
    {
        rpcServer_->stop();
    }
    FibpForwardManager::get()->stop();
}

int FibpProcess::run()
{

    bool caughtException = false;

    try
    {
        LOG(INFO) << "FibpProcess has started";

        boost::thread_group server_group;
        server_group.create_thread(boost::bind(&FiberDriverServerV2::run, driverServer_.get()));
        server_group.create_thread(boost::bind(&FiberHttpServerV2::run, httpServer_.get()));
        rpcServer_->start();

        server_group.join_all();
        LOG(INFO) << "FibpProcess has exited";
        waitSignalThread();
    }
    catch (const std::exception& e)
    {
        caughtException = true;
        LOG(ERROR) << "FibpProcess has aborted by std exception: " << e.what();
    }
    catch (...)
    {
        caughtException = true;
        LOG(ERROR) << "FibpProcess has aborted by unknown exception";
    }

    return caughtException ? 1 : 0;
}
