#include "APIController.h"
#include "RequestParser.h"
#include "ResponseRender.h"
#include <common/Keys.h>
#include <forward-manager/FibpForwardManager.h>
#include <log-manager/FibpLogger.h>
#include <util/driver/Request.h>
#include <util/driver/readers/JsonReader.h>

using namespace izenelib::driver;
namespace fibp
{

APIController::APIController()
    : forward_mgr_(NULL)
{
}

bool APIController::preprocess()
{
    izenelib::driver::Value requestV;
    JsonReader reader;
    if (reader.read(raw_req(), requestV))
    {
        request().assignTmp(requestV);
    }

    forward_mgr_ = FibpForwardManager::get();
    return forward_mgr_ != NULL;
}

void APIController::list_port_forward_services()
{
    std::string agentid;
    RequestParser p(request().get());
    bool ret = p.parse_list_forward_service(agentid);
    if (!ret)
    {
        response().addError("parser request failed.");
        Controller::postprocess();
        return;
    }
    std::vector<ForwardInfoT> infos;
    forward_mgr_->getAllPortForwardServices(agentid, infos);
    ResponseRender::generate_port_forward_services_rsp(infos, response()["ForwardServiceList"]);
}

} // namespace 
