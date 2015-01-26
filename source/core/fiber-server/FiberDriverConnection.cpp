/**
 * @file util/driver/FiberDriverConnection.cpp
 * @author Ian Yang
 * @date Created <2010-05-26 16:12:07>
 */
#include "FiberDriverConnection.h"

#include <util/ClockTimer.h>
#include <util/driver/writers/JsonWriter.h>
#include <util/driver/readers/JsonReader.h>
#include <util/driver/Keys.h>
#include <log-manager/FibpLogger.h>
#include "yield.hpp"
#include <boost/bind.hpp>
#include <iostream>
#include <glog/logging.h>

using namespace boost::asio;
using namespace izenelib;
using namespace izenelib::driver;

static int s_guess_client_num = 0;

namespace fibp {

FiberDriverConnection::FiberDriverConnection(
    io_service& ioService,
    const router_ptr& router,
    fiber_pool_ptr_t fiber_pool
)
: socket_(ioService),
  poller_(socket_),
  router_(router),
  nextContext_(),
  reader_(new izenelib::driver::JsonReader()),
  writer_(new izenelib::driver::JsonWriter()),
  fiber_pool_(fiber_pool)
{
    ++s_guess_client_num;
    if (s_guess_client_num % 10 == 0)
    {
        LOG(INFO) << "guessed driver client num : " << s_guess_client_num;
    }
}

FiberDriverConnection::~FiberDriverConnection()
{
    --s_guess_client_num;
}

void FiberDriverConnection::start()
{
    while(socket_.is_open())
    {
        boost::system::error_code ec;
        boost::asio::async_read(
            socket_,
            nextContext_.formHeaderBuffer(),
            boost::fibers::asio::yield[ec]
            );
        nextContext_.serverTimer.restart();
        if(ec)
        {
            nextContext_.setSequence(0);
            context_ptr context = createContext();
            writeError(context, ec.message());
            break;
        }

        if (nextContext_.sequence() == 0 ||
            nextContext_.formPayloadSize() == 0)
        {
            // header size 0 indicates eof
            LOG(INFO) << "shutdown since received sequence 0!";
            break;
        }
        else if (nextContext_.formPayloadSize() > kLimitSize)
        {
            context_ptr context = createContext();
            writeError(context, "Size exceeds limit.");
            break;
        }
        FIBP_THREAD_MARK_LOG(nextContext_.sequence());
        // allocate enough space to receive payload
        nextContext_.formPayload.resize(nextContext_.formPayloadSize());
        boost::asio::async_read(
            socket_,
            nextContext_.formPayloadBuffer(),
            boost::fibers::asio::yield[ec]
            );
        if (ec)
        {
            context_ptr context = createContext();
            writeError(context, ec.message());
            break;
        }
        context_ptr context = createContext();
        handleRequest(context, ec);
        FIBP_THREAD_MARK_LOG(context->sequence());
    }
    LOG(INFO) << "connection closed.";
    shutdownReceive();
}

void FiberDriverConnection::writeError(context_ptr context, const std::string& errmsg)
{
    context->response.addError(errmsg);
    writeRsp(context);
}

void FiberDriverConnection::writeRsp(context_ptr context)
{
    if (asBool(context->request.header()[driver::Keys::check_time]))
    {
        context->response[driver::Keys::timers][driver::Keys::total_server_time]
            = context->serverTimer.elapsed();
    }
    writer_->write(context->response.get(), context->formPayload);
    context->setFormPayloadSize(context->formPayload.size());

    boost::system::error_code ec;
    boost::asio::async_write(
        socket_,
        context->buffer(),
        boost::fibers::asio::yield[ec]
    );
    if (ec)
    {
        LOG(WARNING) << "write response failed." << ec.message();
    }
}

void FiberDriverConnection::asyncReadFormHeader()
{
    FIBP_THREAD_MARK_LOG(0);
    boost::asio::async_read(
        socket_,
        nextContext_.formHeaderBuffer(),
        boost::bind(&FiberDriverConnection::afterReadFormHeader, shared_from_this(), _1)
    );
}

void FiberDriverConnection::afterReadFormHeader(const boost::system::error_code& e)
{
    // start timer
    nextContext_.serverTimer.restart();

    if (!e)
    {
        if (nextContext_.sequence() == 0 ||
            nextContext_.formPayloadSize() == 0)
        {
            // header size 0 indicates eof
            shutdownReceive();
            LOG(INFO) << "shutdown since received sequence 0!";
        }
        else if (nextContext_.formPayloadSize() > kLimitSize)
        {
            context_ptr context = createContext();
            asyncWriteError(context, "Size exceeds limit.");
            shutdownReceive();
        }
        else
        {
            // allocate enough space to receive payload
            nextContext_.formPayload.resize(nextContext_.formPayloadSize());
            asyncReadFormPayload();
        }
    }
    else
    {
        // sequence has not been read yet, set to 0 explicitly.
        nextContext_.setSequence(0);
        onReadError(e);
    }
}

void FiberDriverConnection::asyncReadFormPayload()
{
    boost::asio::async_read(
        socket_,
        nextContext_.formPayloadBuffer(),
        boost::bind(&FiberDriverConnection::afterReadFormPayload, shared_from_this(), _1)
    );
}

void FiberDriverConnection::afterReadFormPayload(
    const boost::system::error_code& e
)
{
    if (!e)
    {
        context_ptr context = createContext();

        poller_.poll(
            boost::bind(
                &FiberDriverConnection::handleRequest,
                shared_from_this(),
                context,
                boost::asio::placeholders::error
            )
        );

        // read next form
        asyncReadFormHeader();
    }
    else
    {
        onReadError(e);
    }
}

void FiberDriverConnection::handleRequestFunc(
    context_ptr context)
{
    driver::Router::handler_ptr handler = router_->find(
        context->request.controller(),
        context->request.action()
        );

    if (handler)
    {
        try
        {
            // prepare request
            context->request.header()[driver::Keys::remote_ip] =
                socket_.remote_endpoint().address().to_string();

            // prepare response
            context->response.setSuccess(true); // assume success

            if (!handler->is_async())
            {
                izenelib::util::ClockTimer processTimer;

                handler->invoke(context->request,
                    context->response,
                    poller_);

                if (asBool(context->request.header()[driver::Keys::check_time]))
                {
                    context->response[driver::Keys::timers][driver::Keys::process_time]
                        = processTimer.elapsed();
                }

                writeRsp(context);
            }
            else
            {
                FIBP_THREAD_MARK_LOG(0);
                handler->invoke_async(context->request,
                    context->response,
                    poller_,
                    boost::bind(&FiberDriverConnection::writeRsp, shared_from_this(), context));
            }
        }
        catch (const std::exception& e)
        {
            writeError(context, e.what());
        }
    }
    else
    {
        writeError(context, "Handler not found");
    }
}

void FiberDriverConnection::handleRequest(
    context_ptr context,
    const boost::system::error_code& e)
{
    if (!e)
    {
        Value requestValue;
        if (!reader_->read(context->formPayload, requestValue))
        {
            // malformed request
            writeError(context, reader_->errorMessages());
            return;
        }
        if (requestValue.type() != driver::Value::kObjectType)
        {
            // malformed request
            writeError(context,
                "Malformed request: "
                "require an object as input.");
            return;
        }
        context->request.assignTmp(requestValue);

        if (fiber_pool_)
        {
            fiber_pool_->schedule_task(boost::bind(&FiberDriverConnection::handleRequestFunc,
                    shared_from_this(), context));
        }
        else
        {
            handleRequestFunc(context);
        }
    }
    // Error if send end is closed, just ignore it
}

void FiberDriverConnection::asyncWriteResponse(context_ptr context)
{
    FIBP_THREAD_MARK_LOG(0);
    if (asBool(context->request.header()[driver::Keys::check_time]))
    {
        context->response[driver::Keys::timers][driver::Keys::total_server_time]
            = context->serverTimer.elapsed();
    }
    writer_->write(context->response.get(), context->formPayload);
    context->setFormPayloadSize(context->formPayload.size());

    boost::system::error_code ec;
    boost::asio::async_write(
        socket_,
        context->buffer(),
        boost::bind(
            &FiberDriverConnection::afterWriteResponse,
            shared_from_this(),
            context
        )
    );
}

void FiberDriverConnection::asyncWriteError(
    const context_ptr& context,
    const std::string& message
)
{
    context->response.addError(message);

    asyncWriteResponse(context);
}

void FiberDriverConnection::afterWriteResponse(context_ptr)
{}

void FiberDriverConnection::shutdownReceive()
{
    try
    {
        socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_receive);
    }
    catch(const std::exception& e)
    {}
}

void FiberDriverConnection::onReadError(
    const boost::system::error_code& e
)
{
    if (e != boost::asio::error::eof)
    {
        context_ptr context = createContext();
        asyncWriteError(context, e.message());
    }
}

FiberDriverConnection::context_ptr FiberDriverConnection::createContext()
{
    context_ptr context(new izenelib::driver::DriverConnectionContext());
    context->swap(nextContext_);
    return context;
}

FiberDriverConnectionFactory::FiberDriverConnectionFactory(const router_ptr& router)
: router_(router), fiberpool_mgr_(NULL)
{
}

}

