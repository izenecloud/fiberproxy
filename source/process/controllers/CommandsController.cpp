#include "CommandsController.h"
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

CommandsController::CommandsController()
    : forward_mgr_(NULL)
{
}

bool CommandsController::preprocess()
{
    izenelib::driver::Value requestV;
    JsonReader reader;
    if (reader.read(raw_req(), requestV))
    {
        if (requestV.type() == izenelib::driver::Value::kObjectType)
            request().assignTmp(requestV);
    }

    forward_mgr_ = FibpForwardManager::get();
    return forward_mgr_ != NULL;
}

void CommandsController::call_single_service_async()
{
    uint64_t id = FibpLogger::get()->startServiceCall(__FUNCTION__);
    RequestParser p(request().get());

    if (!p.parse_single_api_request(method(), raw_req(), path(), call_api_list_))
    {
        response().addError("parser request data failed.");
        Controller::postprocess();
        return;
    }
    forward_mgr_->call_services_in_fiber(poller().get_io_service(), id,
        call_api_list_, rsp_list_,
        boost::bind(&CommandsController::after_call_single_service, shared_from_this(), id));
}

void CommandsController::call_services_async()
{
    uint64_t id = FibpLogger::get()->startServiceCall(__FUNCTION__);
    RequestParser p(request()[driver::Keys::call_api_list]);
    bool do_transaction = asBool(request()[driver::Keys::do_transaction]);
    if (!p.parse_api_request(call_api_list_))
    {
        response().addError("parser request data failed.");
        Controller::postprocess();
        return;
    }

    //call_api_list_.resize(2);
    //for(std::size_t i = 0; i < call_api_list_.size(); ++i)
    //{
    //    call_api_list_[i].service_name = "local_test";
    //    call_api_list_[i].service_req_data = "{}";
    //    call_api_list_[i].service_type = Raw_Service;
    //}
    //forward_mgr_->call_services_in_fiber(id,
    //    boost::ref(call_api_list_), boost::ref(rsp_list_),
    //    boost::bind(&CommandsController::after_call_services, shared_from_this(), id));
    forward_mgr_->call_services_in_fiber(poller().get_io_service(), id,
        call_api_list_, rsp_list_,
        boost::bind(&CommandsController::after_call_services, shared_from_this(), id),
        do_transaction);
}

void CommandsController::after_call_single_service(uint64_t id)
{
    FIBP_THREAD_MARK_LOG(id);
    ResponseRender r(rsp_list_);
    r.generate_single_rsp(call_api_list_, raw_rsp(), response());
    Controller::postprocess();
    FibpLogger::get()->endServiceCall(id);
}

void CommandsController::after_call_services(uint64_t id)
{
    FIBP_THREAD_MARK_LOG(id);
    ResponseRender r(rsp_list_);
    r.generate_rsp(call_api_list_, response()[driver::Keys::service_rsp_list]);
    Controller::postprocess();
    FibpLogger::get()->endServiceCall(id);
}

void CommandsController::check_alive()
{
    response()["echo"] = "alive";
}

} // namespace 
