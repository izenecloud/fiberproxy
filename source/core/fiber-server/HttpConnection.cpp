#include "HttpConnection.h"
#include "yield.hpp"
#include <util/driver/readers/JsonReader.h>
#include <util/driver/writers/JsonWriter.h>
#include <util/driver/Keys.h>
#include <boost/bind.hpp>
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <iostream>
#include <log-manager/FibpLogger.h>

using namespace izenelib;
namespace fibp
{

static const std::string s_err_rsp_500("HTTP/1.1 500 Internal Server Error\r\n");
static const std::string s_err_rsp_400("HTTP/1.1 400 Bad Request\r\n");
static const std::string s_err_rsp_404("HTTP/1.1 404 Not Found\r\n");
static int s_guess_client_num = 0;

HttpConnection::HttpConnection(boost::asio::io_service& s,
    const router_ptr& router, fiber_pool_ptr_t pool)
: socket_(s)
, poller_(socket_)
, reader_(new izenelib::driver::JsonReader())
, writer_(new izenelib::driver::JsonWriter())
, next_context_()
, router_(router)
, req_parser_(new http::request_parser(socket_, next_context_))
, fiber_pool_(pool)
{
    ++s_guess_client_num;
    if (s_guess_client_num % 10 == 0)
    {
        LOG(INFO) << "guessed http client num : " << s_guess_client_num;
    }
}

HttpConnection::~HttpConnection()
{
    //LOG(INFO) << "a http connection destroyed.";
    --s_guess_client_num;
}

void HttpConnection::start()
{
    req_parser_->init_handler(
        boost::bind(&HttpConnection::request_cb, shared_from_this()),
        boost::bind(&HttpConnection::shutdown, shared_from_this()),
        boost::bind(&HttpConnection::onReadError, shared_from_this(), _1));
    //req_parser_->start_parse();
    req_parser_->do_parse();
}

void HttpConnection::write_error_rsp(const std::string& rsp)
{
    std::string final_rsp = rsp + "Content-Length: 0\r\n\r\n";

    boost::asio::async_write(socket_,
        boost::asio::const_buffers_1(final_rsp.data(), final_rsp.size()),
        boost::fibers::asio::yield);

    shutdown();
}

void HttpConnection::write_rsp(context_ptr context)
{
    if (izenelib::driver::asBool(context->jsonRequest_.header()["check_fibp_time"]))
    {
        context->jsonResponse_[driver::Keys::timers][driver::Keys::total_server_time] = context->serverTimer_.elapsed();
    }
    context->rsp_.code_ = http::OK;
    // we only suppose the response is json if raw body is empty.
    // If the raw body has data, we thought the handler has everything done.
    if (context->rsp_.body_.empty())
    {
        writer_->write(context->jsonResponse_.get(), context->rsp_.body_);
    }
    context->rsp_.headers_.push_back(std::make_pair("ContentType", "application/json"));
    if (context->req_.keep_alive_)
    {
        char buf[100];
        context->rsp_.headers_.push_back(std::make_pair("Connection", "keep-alive"));
        sprintf(buf, "timeout=%d", 100);
        context->rsp_.headers_.push_back(std::make_pair("Keep-Alive", buf));
    }
    else
    {
        context->rsp_.headers_.push_back(std::make_pair("Connection", "close"));
    }
    std::ostringstream oss;
    oss << context->rsp_;
    boost::asio::async_write(socket_,
        boost::asio::const_buffers_1(oss.str().data(), oss.str().size()),
        boost::fibers::asio::yield);
    FIBP_THREAD_MARK_LOG(0);

    after_write(context);
}

void HttpConnection::async_write_error_rsp(const std::string& rsp)
{
    std::string final_rsp = rsp + "Content-Length: 0\r\n\r\n";

    boost::asio::async_write(socket_,
        boost::asio::const_buffers_1(final_rsp.data(), final_rsp.size()),
        boost::bind(&HttpConnection::shutdown, shared_from_this()));
}

void HttpConnection::async_write_rsp(context_ptr context)
{
    if (izenelib::driver::asBool(context->jsonRequest_.header()["check_fibp_time"]))
    {
        context->jsonResponse_[driver::Keys::timers][driver::Keys::total_server_time] = context->serverTimer_.elapsed();
    }
    context->rsp_.code_ = http::OK;

    // we only suppose the response is json if raw body is empty.
    // If the raw body has data, we thought the handler has everything done.
    if (context->rsp_.body_.empty())
    {
        writer_->write(context->jsonResponse_.get(), context->rsp_.body_);
    }
    context->rsp_.headers_.push_back(std::make_pair("ContentType", "application/json"));
    if (context->req_.keep_alive_)
    {
        char buf[100];
        context->rsp_.headers_.push_back(std::make_pair("Connection", "keep-alive"));
        sprintf(buf, "timeout=%d", 100);
        context->rsp_.headers_.push_back(std::make_pair("Keep-Alive", buf));
    }
    else
    {
        context->rsp_.headers_.push_back(std::make_pair("Connection", "close"));
    }
    std::ostringstream oss;
    oss << context->rsp_;
    boost::asio::async_write(socket_,
        boost::asio::const_buffers_1(oss.str().data(), oss.str().size()),
        boost::bind(&HttpConnection::after_write, shared_from_this(), context));
    FIBP_THREAD_MARK_LOG(0);
}

void HttpConnection::after_write(context_ptr context)
{
    FIBP_THREAD_MARK_LOG(0);
    if (context->req_.keep_alive_)
    {
    }
    else
    {
        shutdown();
    }
}

void HttpConnection::shutdown()
{
    try
    {
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_receive);
        //LOG(INFO) << "closing client";
    }
    catch(const std::exception& e)
    {
        LOG(INFO) << e.what();
    }
    req_parser_.reset();
}

void HttpConnection::onReadError(const boost::system::error_code& ec)
{
    if (ec != boost::asio::error::eof)
    {
        context_ptr context = createContext();
        write_error_rsp(s_err_rsp_500);
        LOG(ERROR) << "read error: " << ec.message();
    }
}

bool HttpConnection::request_cb()
{
    context_ptr c = createContext();
    return handleRequest(c) && c->req_.keep_alive_;
}

bool HttpConnection::handleRequestFunc(const std::string& controller,
    const std::string& action, context_ptr context)
{
    //LOG(INFO) << "handle in fiber : " << boost::this_fiber::get_id() << " for conn:" << context.get();
    izenelib::driver::Router::handler_ptr handler = router_->find(
        controller, action);
    if (!handler)
    {
        write_error_rsp(s_err_rsp_404);
        LOG(INFO) << "handler not found for : " << controller
            << "-" << action;
        return false;
    }
    try
    {
        context->jsonResponse_.setSuccess(true);
        context->rsp_.clear();
        std::string path = context->req_.path_;
        if (!context->req_.query_.empty())
        {
            path += "?" + context->req_.query_;
        }

        if (handler->is_async())
        {
            handler->invoke_raw_async(
                context->jsonRequest_,
                context->jsonResponse_,
                context->req_.body_,
                context->rsp_.body_,
                poller_,
                boost::bind(&HttpConnection::write_rsp, shared_from_this(), context),
                path,
                context->req_.method_
                );
        }
        else
        {
            handler->invoke_raw(
                context->jsonRequest_,
                context->jsonResponse_,
                context->req_.body_,
                context->rsp_.body_,
                poller_,
                path,
                context->req_.method_
                );
            write_rsp(context);
        }
    }
    catch(const std::exception& e)
    {
        LOG(ERROR) << "exception in handler: " << e.what();
        write_error_rsp(s_err_rsp_500);
        return false;
    }
    return true;
}

// called when a http session finished parsing.
bool HttpConnection::handleRequest(context_ptr context)
{
    FIBP_THREAD_MARK_LOG(0);
    // get controller and action from url path.
    std::string controller;
    std::string action;
    if (context->req_.path_.empty())
    {
        write_error_rsp(s_err_rsp_400);
        return false;
    }
    std::vector<std::string> elems;
    boost::trim_if(context->req_.path_, boost::is_any_of("/"));
    boost::split(elems, context->req_.path_, boost::is_any_of("/"), boost::token_compress_on);
    if (elems.size() < 1)
    {
        write_error_rsp(s_err_rsp_400);
        return false;
    }
    controller = elems.at(0);
    if (elems.size() > 1)
    {
        action = elems.at(1);
    }

    if (fiber_pool_)
    {
        //LOG(INFO) << "schedule_task for " << context.get();
        fiber_pool_->schedule_task(boost::bind(&HttpConnection::handleRequestFunc,
                shared_from_this(),
                controller,
                action,
                context));
        return true;
    }
    return handleRequestFunc(controller, action, context);
}

HttpConnection::context_ptr HttpConnection::createContext()
{
    context_ptr context(new http::session_t());
    context->swap(next_context_);
    return context;
}

HttpConnectionFactory::HttpConnectionFactory(const router_ptr& router)
: router_(router), fiberpool_mgr_(NULL)
{
}

}

