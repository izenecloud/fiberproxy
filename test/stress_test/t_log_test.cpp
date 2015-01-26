#include <log-manager/FibpLogger.h>
#include <glog/logging.h>
#include <boost/thread.hpp>

using namespace fibp;

void log_test()
{
    int cnt = 1000000;
    while (--cnt > 0)
    {
        uint64_t id = FibpLogger::get()->startServiceCall("");
        FibpLogger::get()->sendServiceRequest(id, "test1", "localhost", "8888");
        FibpLogger::get()->sendServiceRequest(id, "test2", "localhost", "8888");
        sleep(1);
        FibpLogger::get()->getServiceRsp(id, "test1");
        FibpLogger::get()->sendServiceRequest(id, "test3", "localhost", "8888");
        FibpLogger::get()->getServiceRsp(id, "test2");
        FibpLogger::get()->getServiceRsp(id, "test3");
        FibpLogger::get()->sendServiceRequest(id, "test4", "localhost", "8888");
        sleep(1);
        FibpLogger::get()->getServiceRsp(id, "test4");
        FibpLogger::get()->endServiceCall(id);
        if (cnt % 100000 == 0)
        {
            LOG(INFO) << "log test : " << cnt;
            sleep(1);
        }
    }
    LOG(INFO) << "test finished.";
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        LOG(INFO) << "ip port needed.";
        return -1;
    }
    std::string dest_ip = argv[1];
    std::string dest_port = argv[2];
    LOG(INFO) << "doing the log test.";
    FibpLogger::get()->init(dest_ip, dest_port, "test_log_service");
    boost::thread_group thread_list;
    std::size_t num = 10;
    for(std::size_t i = 0; i < num; ++i)
    {
        thread_list.create_thread(boost::bind(&log_test));
    }
    thread_list.join_all();
    return 0;
}
