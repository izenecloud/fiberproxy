#ifndef FIBP_TRANSACTION_MGR_H
#define FIBP_TRANSACTION_MGR_H

#include <string>
namespace fibp
{
class FibpClientMgr;
class FibpTransactionMgr
{

public:
    FibpTransactionMgr();
    ~FibpTransactionMgr(){}

    std::string get_transaction_id(const std::string& service_rps);
    bool confirm(FibpClientMgr* client_mgr, const std::string& host, const std::string& port,
        const std::string& api, const std::string& tran_id);
    bool cancel(FibpClientMgr* client_mgr, const std::string& host, const std::string& port,
        const std::string& api, const std::string& tran_id);

private:
    bool send_transaction_api(FibpClientMgr* client_mgr, const std::string& host, const std::string& port,
        const std::string& api, const std::string& tran_id, const std::string& tran_action);

};

}

#endif
