#include "FibpTransactionMgr.h"
#include "FibpClientMgr.h"
#include <common/FibpCommonTypes.h>
#include <log-manager/FibpLogger.h>
#include <glog/logging.h>

namespace fibp
{

FibpTransactionMgr::FibpTransactionMgr()
{
}

std::string FibpTransactionMgr::get_transaction_id(const std::string& service_rps)
{
    std::string tran_key("\"transaction_id\"");
    std::size_t pos = service_rps.find(tran_key);
    std::string tran_id;
    if (pos == std::string::npos)
    {
        return tran_id;
    }
    std::size_t id_start_pos, id_end_pos;
    id_start_pos = service_rps.find("\"", pos + tran_key.length(), 10);
    if (id_start_pos == std::string::npos)
        return tran_id;
    id_end_pos = service_rps.find("\"", id_start_pos + 1, 128);
    if (id_end_pos == std::string::npos)
    {
        return tran_id;
    }
    tran_id = service_rps.substr(id_start_pos + 1, id_end_pos - id_start_pos - 1);
    LOG(INFO) << "found transaction id: " << tran_id;
    return tran_id;
}

bool FibpTransactionMgr::confirm(FibpClientMgr* client_mgr, const std::string& host, const std::string& port,
    const std::string& api, const std::string& tran_id)
{
    return send_transaction_api(client_mgr, host, port, api, tran_id, "/confirm");
}

bool FibpTransactionMgr::cancel(FibpClientMgr* client_mgr, const std::string& host, const std::string& port,
    const std::string& api, const std::string& tran_id)
{
    return send_transaction_api(client_mgr, host, port, api, tran_id, "/cancel");
}

bool FibpTransactionMgr::send_transaction_api(FibpClientMgr* client_mgr, const std::string& host, const std::string& port,
    const std::string& api, const std::string& tran_id, const std::string& tran_action)
{
    std::string postdata;
    std::string tran_key("\"transaction_id\"");
    postdata = "{" + tran_key + ":" + "\"" + tran_id + "\"" + "}";
    uint32_t timeout = 1000*10;
    FibpHttpClientPtr client = client_mgr->send_request(api + tran_action, http::POST, host, port,
        postdata, timeout);
    if (!client)
    {
        FibpLogger::get()->logTransaction(false, api, tran_action, tran_id, "Send Failed.");
        return false;
    }
    bool can_retry = false;
    std::string rsp;
    bool ret = client_mgr->get_response(client, rsp, can_retry);

    if (ret)
    {
        FibpLogger::get()->logTransaction(true, api, tran_action, tran_id, rsp);
    }
    else
    {
        FibpLogger::get()->logTransaction(false, api, tran_action, tran_id, rsp);
    }
    return ret;
}

}

