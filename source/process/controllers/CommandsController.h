#ifndef PROCESS_CONTROLLERS_COMMANDS_CONTROLLER_H
#define PROCESS_CONTROLLERS_COMMANDS_CONTROLLER_H
#include <util/driver/Controller.h>
#include <string>
#include <common/FibpCommonTypes.h>
#include <boost/enable_shared_from_this.hpp>

namespace fibp
{

class FibpForwardManager;
class CommandsController : public izenelib::driver::Controller,
    public boost::enable_shared_from_this<CommandsController>
{
public:
    CommandsController();
    // call multi services together.
    void call_services_async();
    // return the detail of the require service name include the define of 
    // request format and response format.
    void query_services();
    // return all registered services name.
    void list_services();
    void call_single_service_async();
    void check_alive();

    bool preprocess();

private:
    void after_call_services(uint64_t id);
    void after_call_single_service(uint64_t id);
    FibpForwardManager* forward_mgr_;
    ServicesRsp rsp_list_;
    std::vector<ServiceCallReq> call_api_list_;
};

} // namespace 

#endif // PROCESS_CONTROLLERS_COMMANDS_CONTROLLER_H
