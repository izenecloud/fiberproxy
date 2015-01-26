#include <3rdparty/msgpack/msgpack.hpp>
#include <3rdparty/msgpack/rpc/server.h>
#include <glog/logging.h>
#include <string>
#include <boost/lexical_cast.hpp>

struct TestRpcData
{
    int t1;
    std::string tstr;
    std::vector<int> tvec;
    MSGPACK_DEFINE(t1, tstr, tvec);
};

static TestRpcData test_reqdata;
static const std::string ERROR_TEST_RSP("error_test_msg");

class RpcTestServer: public msgpack::rpc::server::base
{
public:
    RpcTestServer()
    {
    }
    ~RpcTestServer()
    {
    }
    void start()
    {
        instance.listen("10.10.99.131", 9999);
        instance.start(16);
    }
    void stop()
    {
        instance.end();
        instance.join();
    }
    void dispatch(msgpack::rpc::request req)
    {
        try
        {
            std::string method;
            req.method().convert(&method);
            //LOG(INFO) << "rpc method: " << method;
            if (method == "test")
            {
                req.result(true);
            }
            else if (method == "test_rpc_call_from_http")
            {
                msgpack::type::tuple<std::string> params;
                req.params().convert(&params);
                const std::string& http_body = params.get<0>();
                //LOG(INFO) << "got http in rpc server: " << http_body;
                req.result(http_body);
            }
            else if (method == "test_rpc_call")
            {
                msgpack::type::tuple<TestRpcData> params;
                req.params().convert(&params);
                TestRpcData& reqdata = params.get<0>();
                bool issame = (test_reqdata.tvec == reqdata.tvec);
                BOOST_ASSERT(issame);
                //LOG(INFO) << "got test request data: " << reqdata.t1 << "," <<
                //    reqdata.tstr << "," << reqdata.tvec.size() << ", " << issame;
                req.result(reqdata);
            }
            else if (method == "test_rpc_error")
            {
                msgpack::type::tuple<TestRpcData> params;
                req.params().convert(&params);
                TestRpcData& reqdata = params.get<0>();
                bool issame = (test_reqdata.tvec == reqdata.tvec);
                BOOST_ASSERT(issame);
                //LOG(INFO) << "got test request data: " << reqdata.t1 << "," <<
                //    reqdata.tstr << "," << reqdata.tvec.size() << ", " << issame;
                req.result(ERROR_TEST_RSP);
            }
            else
            {
                req.error(std::string("NO_METHOD_ERROR"));
            }
        }
        catch(const std::exception& e)
        {
            LOG(INFO) << e.what();
            req.error(std::string(e.what()));
        }
    }
};

struct ServiceCallReq
{
    std::string service_name;
    std::string service_api;
    std::string service_req_data;
    std::string service_cluster;
    int service_type;
    bool enable_cache;
    ServiceCallReq()
        :service_type(0), enable_cache(false)
    {
    }
    MSGPACK_DEFINE(service_name, service_api, service_req_data, service_cluster,
        service_type, enable_cache);
};
struct ServiceCallRsp
{
    std::string service_name;
    std::string rsp;
    std::string error;
    bool is_cached;
    ServiceCallRsp()
        :is_cached(false)
    {
    }
    MSGPACK_DEFINE(service_name, rsp, error, is_cached);
};
struct RpcServicesReq
{
    std::vector<ServiceCallReq> reqlist;
    MSGPACK_DEFINE(reqlist);
};
struct RpcServicesRsp
{
    std::vector<ServiceCallRsp> rsplist;
    MSGPACK_DEFINE(rsplist);
};

void rpc_stress_test(msgpack::rpc::session& s, const RpcServicesReq& rpcreq)
{
    int cnt = 10000;
    while(--cnt > 0)
    {
        RpcServicesRsp rpcrsp;
        try
        {
            rpcrsp = s.call("call_services_async", rpcreq).get<RpcServicesRsp>();
        }
        catch(const std::exception& e)
        {
            LOG(INFO) << "exception get rpc return. " << e.what();
            return;
        }

        TestRpcData rsp_testdata;
        msgpack::unpacked rspmsg;
        msgpack::object obj;
        if (rpcrsp.rsplist[0].error.empty())
        {
            msgpack::unpack(&rspmsg, rpcrsp.rsplist[0].rsp.data(), rpcrsp.rsplist[0].rsp.size());
            obj = rspmsg.get();
            obj.convert(&rsp_testdata);
        }
        else
        {
            LOG(INFO) << "service error: " << rpcrsp.rsplist[0].error;
        }
    }
    LOG(INFO) << "rpc stress test finished in thread.";
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        LOG(INFO) << "fibp ip port needed.";
        return -1;
    }
    std::string dest_ip = argv[1];
    std::string dest_port = argv[2];

    test_reqdata.t1 = 12;
    test_reqdata.tstr = "1234";
    test_reqdata.tvec.push_back(12);
    test_reqdata.tvec.push_back(15);
    test_reqdata.tvec.push_back(17);

    msgpack::type::tuple<TestRpcData> reqparams(test_reqdata);
    msgpack::sbuffer buf;
    msgpack::pack(buf, reqparams);

    RpcTestServer server;
    server.start();
    LOG(INFO) << "begin test fibp rpc.";
    sleep(3);

    RpcServicesReq rpcreq;
    rpcreq.reqlist.resize(1);

    // rpc -> rpc test
    rpcreq.reqlist[0].service_name = "rpc_test_service";
    rpcreq.reqlist[0].service_api = "test_rpc_call";
    rpcreq.reqlist[0].service_req_data.assign(buf.data(), buf.size());
    LOG(INFO) << "req_data size: " << rpcreq.reqlist[0].service_req_data.size();
    rpcreq.reqlist[0].service_cluster = "dev";
    rpcreq.reqlist[0].service_type = 1;

    // rpc -> rpc error test
    //rpcreq.reqlist[1].service_name = "rpc_test_service";
    //rpcreq.reqlist[1].service_api = "test_rpc_error";
    //rpcreq.reqlist[1].service_req_data.assign(buf.data(), buf.size());
    //rpcreq.reqlist[1].service_cluster = "dev";
    //rpcreq.reqlist[1].service_type = 1;

    // rpc -> http test
    //rpcreq.reqlist[2].service_name = "sf1r";
    //rpcreq.reqlist[2].service_api = "/sf1r/documents/get_doc_count";
    //rpcreq.reqlist[2].service_req_data = "{\"collection\":\"tuana\"}";
    //rpcreq.reqlist[2].service_cluster = "dev";
    //rpcreq.reqlist[2].service_type = 0;

    msgpack::rpc::session_pool pool;
    pool.start(4);

    sleep(1);
    msgpack::rpc::session s = pool.get_session(dest_ip, boost::lexical_cast<uint16_t>(dest_port));
    // test simple single rpc call

    TestRpcData single_req;
    single_req.t1 = 111;
    single_req.tstr = "11111";
    single_req.tvec.push_back(11);
    single_req.tvec.push_back(111);
    std::string call_single_rpc_str = "call_single_service_async/";
    call_single_rpc_str += "rpc_test_service/";
    call_single_rpc_str += "test_rpc_call";
    try
    {
        TestRpcData single_rsp = s.call(call_single_rpc_str, single_req).get<TestRpcData>();
        bool issame = single_req.tvec == single_rsp.tvec;
        LOG(INFO) << "single rpc request returned: " << single_rsp.t1 <<
            ", " << single_rsp.tstr << ", issame : " << issame;
    }
    catch(const std::exception& e)
    {
        LOG(INFO) << "call single rpc error:" << e.what();
    }

    sleep(3);
    boost::thread_group test_threads;
    for(std::size_t i = 0; i < 10; ++i)
    {
        test_threads.create_thread(boost::bind(&rpc_stress_test, boost::ref(s), boost::ref(rpcreq)));
    }

    RpcServicesRsp rpcrsp;
    try
    {
        rpcrsp = s.call("call_services_async", rpcreq).get<RpcServicesRsp>();
        LOG(INFO) << "rpc returned." << rpcrsp.rsplist.size();
    }
    catch(const std::exception& e)
    {
        LOG(INFO) << "exception get rpc return. " << e.what();
        server.stop();
        return -1;
    }

    sleep(1);
    BOOST_ASSERT(rpcrsp.rsplist.size() == rpcreq.reqlist.size());
    BOOST_ASSERT(rpcrsp.rsplist[0].service_name == rpcreq.reqlist[0].service_name);

    TestRpcData rsp_testdata;
    msgpack::unpacked rspmsg;
    msgpack::object obj;
    if (rpcrsp.rsplist[0].error.empty())
    {
        msgpack::unpack(&rspmsg, rpcrsp.rsplist[0].rsp.data(), rpcrsp.rsplist[0].rsp.size());
        obj = rspmsg.get();
        obj.convert(&rsp_testdata);
        bool issame = rsp_testdata.tvec == rsp_testdata.tvec;
        BOOST_ASSERT(issame);
        LOG(INFO) << "got test response data from fibp: " << rsp_testdata.t1 << "," <<
            rsp_testdata.tstr << "," << rsp_testdata.tvec.size() << ", " << issame;
    }
    else
    {
        LOG(INFO) << "service error: " << rpcrsp.rsplist[0].error;
    }

    //LOG(INFO) << "service error test returned : " << rpcrsp.rsplist[1].error;

    // http -> rpc response.
    //if (rpcrsp.rsplist[2].error.empty())
    //    LOG(INFO) << "http response is : " << rpcrsp.rsplist[2].rsp;
    //else
    //    LOG(INFO) << "http error is : " << rpcrsp.rsplist[2].error;

    test_threads.join_all();
    printf("press any key to exit.");
    char c = getchar();
    server.stop();
}
