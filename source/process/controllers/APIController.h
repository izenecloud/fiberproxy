#ifndef PROCESS_CONTROLLERS_API_CONTROLLER_H
#define PROCESS_CONTROLLERS_API_CONTROLLER_H
#include <util/driver/Controller.h>
#include <string>
#include <common/FibpCommonTypes.h>
#include <boost/enable_shared_from_this.hpp>

namespace fibp
{

class FibpForwardManager;
class APIController : public izenelib::driver::Controller,
    public boost::enable_shared_from_this<APIController>
{
public:
    APIController();
    void list_port_forward_services();
    void check_alive();

    bool preprocess();

private:
    FibpForwardManager* forward_mgr_;
};

} // namespace 

#endif // 
