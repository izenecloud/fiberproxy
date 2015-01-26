#ifndef ASYNC_MULTI_IOSERVICES_SERVER_H
#define ASYNC_MULTI_IOSERVICES_SERVER_H
#include <boost/asio.hpp>
#include <boost/noncopyable.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include "loop.hpp"
#include <glog/logging.h>

namespace fibp {

class io_service_pool : private boost::noncopyable
{
public:
    explicit io_service_pool(std::size_t pool_size)
        : next_io_service_(0)
    {
        main_io_service_.reset(new boost::asio::io_service);
        work_ptr w(new boost::asio::io_service::work(*main_io_service_));
        work_.push_back(w);
        if (pool_size <= 1)
            return;
        for(std::size_t i = 0; i < pool_size; ++i)
        {
            io_service_ptr s(new boost::asio::io_service);
            work_ptr w(new boost::asio::io_service::work(*s));
            io_services_.push_back(s);
            work_.push_back(w);
        }
    }

    void run()
    {
        if (io_services_.size() == 0)
        {
            io_service_run(main_io_service_);
            return;
        }
        boost::thread_group running_threads;
        for(std::size_t i = 0; i < io_services_.size(); ++i)
        {
            running_threads.create_thread(
                boost::bind(&io_service_pool::io_service_run,
                    io_services_[i])
                );
        }
        running_threads.create_thread(boost::bind(&boost::asio::io_service::run, main_io_service_));
        LOG(INFO) << "all io_service begin running.";
        running_threads.join_all();
    }

    void stop()
    {
        for(std::size_t i = 0; i < io_services_.size(); ++i)
        {
            io_services_[i]->stop();
        }
        main_io_service_->stop();
    }

    void reset()
    {
        for(std::size_t i = 0; i < io_services_.size(); ++i)
        {
            if (io_services_[i]->stopped())
            {
                io_services_[i]->reset();
            }
        }
        if (main_io_service_->stopped())
        {
            main_io_service_->reset();
        }
    }

    boost::asio::io_service& get_io_service()
    {
        if (io_services_.empty())
            return *main_io_service_;
        return *io_services_[++next_io_service_ % io_services_.size()];
    }

    boost::asio::io_service& get_main_io_service()
    {
        return *main_io_service_;
    }

private:
    typedef boost::shared_ptr<boost::asio::io_service> io_service_ptr;
    typedef boost::shared_ptr<boost::asio::io_service::work> work_ptr;

    static void io_service_run(io_service_ptr io_service)
    {
        //io_service->run();
        boost::fibers::fiber f(boost::bind(boost::fibers::asio::run_service,
                boost::ref(*io_service)));
        f.join();
    }

    io_service_ptr  main_io_service_;
    std::vector<io_service_ptr> io_services_;
    std::vector<work_ptr> work_;
    std::size_t next_io_service_;
};

template<typename ConnectionFactory>
class AsyncMultiIOServicesServer : private boost::noncopyable
{
public:
    typedef typename ConnectionFactory::connection_type connection_type;
    typedef boost::shared_ptr<connection_type> connection_ptr;

    typedef boost::function1<void, const boost::system::error_code&> error_handler;

    /**
     * @brief Construct a server listening on the specified TCP address and
     * port.
     * @param address listen address
     * @param port listen port
     */
    explicit AsyncMultiIOServicesServer(
        const boost::asio::ip::tcp::endpoint& bindPort,
        const boost::shared_ptr<ConnectionFactory>& connectionFactory,
        std::size_t io_pool_size
    );

    ~AsyncMultiIOServicesServer(){}

    void init();
    inline void run();
    inline void reset();
    inline void stop();
    inline void setErrorHandler(error_handler handler);
    inline boost::asio::ip::tcp::endpoint getBindedEndpoint()
    {
        return acceptor_.local_endpoint();
    }

    inline void listen();
    inline void asyncAccept();
private:
    inline void onError(const boost::system::error_code& e);
    inline void onAccept(const boost::system::error_code& e);
    static void runConnection(connection_ptr conn);

    io_service_pool service_pool_;
    boost::asio::ip::tcp::endpoint bindPort_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::shared_ptr<ConnectionFactory> connectionFactory_;
    connection_ptr newConnection_;
    error_handler errorHandler_;
};

template<typename ConnectionFactory>
AsyncMultiIOServicesServer<ConnectionFactory>::AsyncMultiIOServicesServer(
    const boost::asio::ip::tcp::endpoint& bindPort,
    const boost::shared_ptr<ConnectionFactory>& connectionFactory,
    std::size_t io_pool_size
)
: service_pool_(io_pool_size)
, bindPort_(bindPort)
, acceptor_(service_pool_.get_main_io_service())
, connectionFactory_(connectionFactory)
, newConnection_()
, errorHandler_()
{
}

template<typename ConnectionFactory>
void AsyncMultiIOServicesServer<ConnectionFactory>::init(
    )
{
    listen();
    asyncAccept();
}

template<typename ConnectionFactory>
void AsyncMultiIOServicesServer<ConnectionFactory>::run()
{
    service_pool_.run();
}

template<typename ConnectionFactory>
void AsyncMultiIOServicesServer<ConnectionFactory>::reset()
{
    service_pool_.reset();
}

template<typename ConnectionFactory>
void AsyncMultiIOServicesServer<ConnectionFactory>::stop()
{
    service_pool_.stop();
}

template<typename ConnectionFactory>
void AsyncMultiIOServicesServer<ConnectionFactory>::setErrorHandler(error_handler handler)
{
    errorHandler_ = handler;
}

template<typename ConnectionFactory>
void AsyncMultiIOServicesServer<ConnectionFactory>::listen(
)
{
    acceptor_.open(bindPort_.protocol());
    acceptor_.set_option(boost::asio::ip::tcp::acceptor::reuse_address(true));
    acceptor_.bind(bindPort_);
    acceptor_.listen();
}

template<typename ConnectionFactory>
void AsyncMultiIOServicesServer<ConnectionFactory>::asyncAccept()
{
    newConnection_.reset(
        connectionFactory_->create(service_pool_.get_io_service())
        );
    acceptor_.async_accept(
        newConnection_->socket(),
        boost::bind(&AsyncMultiIOServicesServer<ConnectionFactory>::onAccept, this,  _1)
        );
}

template<typename ConnectionFactory>
void AsyncMultiIOServicesServer<ConnectionFactory>::onAccept(const boost::system::error_code& e)
{
    if (!e)
    {
        newConnection_->socket().get_io_service().post(
            boost::bind(&runConnection, newConnection_));
        asyncAccept();
    }
    else
    {
        onError(e);
    }
}

template<typename ConnectionFactory>
void AsyncMultiIOServicesServer<ConnectionFactory>::runConnection(connection_ptr conn)
{
    boost::fibers::fiber f(boost::bind(&connection_type::start, conn));
    f.detach();
}

template<typename ConnectionFactory>
void AsyncMultiIOServicesServer<ConnectionFactory>::onError(const boost::system::error_code& e)
{
    if (errorHandler_)
    {
        errorHandler_(e);
    }
}

} // namespace 

#endif // 
