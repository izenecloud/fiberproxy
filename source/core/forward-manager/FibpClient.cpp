#include "FibpClient.h"
#include "FibpClientFuture.h"
#include <boost/fiber/all.hpp>
#include <boost/bind.hpp>
#include <fiber-server/yield.hpp>
#include <glog/logging.h>
#include <3rdparty/msgpack/msgpack.hpp>

namespace ba = boost::asio;
namespace bs = boost::system;

namespace fibp
{

static const std::string TIMEOUT_ERR("Server Timed Out.");
static const std::string SERVER_RSP_TOO_LARGE_ERR("Server Response Too Large.");

static const uint8_t RPC_REQ = 0;
static const uint8_t RPC_RSP = 1;
struct msg_rpc
{
    uint8_t type;
    msg_rpc()
        :type(RPC_REQ)
    {
    }
    MSGPACK_DEFINE(type);
};

struct msg_request
{
    msg_request()
        :type(RPC_REQ), msgid(0)
    {
    }
    void pack(msgpack::packer<msgpack::sbuffer>& pk) const
    {
        pk.pack_array(4);
        pk.pack(type);
        pk.pack(msgid);
        pk.pack(method);
        pk.pack_raw_body(packed_param.data(), packed_param.size());
    }
    uint8_t type;
    uint32_t msgid;
    std::string method;
    std::string packed_param;
};

struct msg_response
{
    msg_response()
        :type(RPC_RSP), msgid(0)
    {
    }
    uint8_t type;
    uint32_t msgid;
    msgpack::object err;
    msgpack::object result;
    MSGPACK_DEFINE(type, msgid, err, result);
};

ClientSession::ClientSession(boost::asio::io_service& service, const std::string& host, const std::string& port)
    : socket_(service), conn_to_(5000), read_to_(5000), io_(service), deadline_(service), host_(host), port_(port)
{
    connecting_ = false;
    deadline_.expires_at(boost::posix_time::pos_infin);
}

bool ClientSession::send_data(const std::string& reqdata)
{
    while (connecting_)
    {
        boost::this_fiber::yield();
    }
    if (!socket_.is_open())
    {
        bs::error_code ec;
        ec = async_connect();
        if (ec)
        {
            return false;
        }
    }
    return async_write(reqdata);
}

void ClientSession::prepare_timeout(int msec)
{
    if (msec == 0)
        return;
    deadline_.expires_from_now(boost::posix_time::milliseconds(msec));
    deadline_.async_wait(boost::bind(&ClientSession::check_deadline, this, _1));
}

void ClientSession::clear_timeout()
{
    deadline_.expires_at(boost::posix_time::pos_infin);
    deadline_.cancel();
}

void ClientSession::check_deadline(const boost::system::error_code& ec)
{
    if (ec == boost::asio::error::operation_aborted)
    {
        deadline_.expires_at(boost::posix_time::pos_infin);
        return;
    }
    if (deadline_.expires_at() <= boost::asio::deadline_timer::traits_type::now())
    {
        //LOG(INFO) << "deadline happened.";
        try
        {
            socket_.cancel();
            if (socket_.is_open())
            {
                shutdown(true);
            }
            deadline_.expires_at(boost::posix_time::pos_infin);
        }
        catch(const boost::system::system_error& e)
        {
            LOG(ERROR) << "check deadline error: " << e.what();
        }
    }
}

bs::error_code ClientSession::async_connect()
{
    connecting_ = true;
    bool ret = false;
    bs::error_code ec;
    try
    {
        ba::ip::tcp::resolver resolver(io_);
        ba::ip::tcp::resolver::query q(host_, port_);
        ba::ip::tcp::resolver::iterator iter = resolver.resolve(q);

        prepare_timeout(conn_to_);

        time_t s = time(NULL);
        ba::async_connect(socket_, iter, boost::fibers::asio::yield[ec]);
        ret = socket_.is_open() && !ec;
        time_t e = time(NULL);
        if (e - s > 1)
            LOG(INFO) << "connected: " << host_ << ":" << port_ << ", fd: " << socket_.native_handle() << ", used: " << e-s;
    }
    catch(const std::exception& e)
    {
        ret = false;
    }

    clear_timeout();
    if (!ret)
    {
        LOG(INFO) << "connect to host failed." << host_ << ":" << port_;
        shutdown(true);
    }
    connecting_ = false;
    return ec;
}

bool ClientSession::async_write(const std::string& reqdata)
{
    try
    {
        bs::error_code ec;
        ba::async_write(socket_, boost::asio::const_buffers_1(reqdata.data(), reqdata.size()),
            boost::fibers::asio::yield[ec]);

        if (ec)
        {
            if (!connecting_)
            {
                LOG(WARNING) << "write request data failed." << ec.message();
                shutdown(true);
            }
            return false;
        }
    }
    catch(const std::exception& e)
    {
        LOG(ERROR) << "write error: " << e.what();
        shutdown(true);
        return false;
    }
    return true;
}

bs::error_code ClientSession::async_read_some(const ba::mutable_buffers_1& buf, std::size_t bufsize, std::size_t& bytes_read)
{
    if (read_to_ > 0)
    {
        prepare_timeout(read_to_ + bufsize/1024/1024);
    }
    bs::error_code ec;
    bytes_read = socket_.async_read_some(buf,
        boost::fibers::asio::yield[ec]);

    if (ec && ec != boost::asio::error::operation_aborted)
    {
        shutdown(ec == boost::asio::error::eof);
        bytes_read = 0;
    }

    clear_timeout();
    return ec;
}

bs::error_code ClientSession::async_read(const ba::mutable_buffers_1& buf, std::size_t bufsize)
{
    if (read_to_ > 0)
        prepare_timeout(read_to_ + bufsize/1024/1024);

    bs::error_code ec;
    ba::async_read(socket_, buf,
        boost::fibers::asio::yield[ec]);

    if (ec && ec != boost::asio::error::operation_aborted)
    {
        shutdown(ec == boost::asio::error::eof);
    }

    clear_timeout();
    return ec;
}

void ClientSession::shutdown(bool need_close)
{
    try
    {
        clear_timeout();
        socket_.shutdown(ba::ip::tcp::socket::shutdown_receive);
        if (need_close)
        {
            socket_.close();
        }
    }
    catch(const std::exception& e)
    {
        socket_.close();
    }
}

FibpClientBase::FibpClientBase(boost::asio::io_service& io_service, const std::string& host, const std::string& port)
    : session_(new ClientSession(io_service, host, port))
{

}

std::string FibpClientBase::host() const
{
    return session_->host();
}

std::string FibpClientBase::port() const
{
    return session_->port();
}


FibpHttpClient::FibpHttpClient(boost::asio::io_service& io_service, const std::string& host, const std::string& port)
    : session_(new ClientSession(io_service, host, port)), rsp_parser_(session_->socket_, next_rsp_), can_retry_(true)
{
}

std::string FibpHttpClient::host() const
{
    return session_->host();
}

std::string FibpHttpClient::port() const
{
    return session_->port();
}


bool FibpHttpClient::send_http_request(
    http::request_t& http_req,
    int timeout_ms)
{
    can_retry_ = true;
    session_->set_timeout(timeout_ms*2, timeout_ms);

    http_req.headers_.push_back(std::make_pair("Host", session_->host()+":"+session_->port()));
    request_stream_.clear();
    request_stream_.str(std::string());
    request_stream_ << http_req;

    //LOG(INFO) << "sending to: " << session_->host() << ":" << session_->port() <<
    //    ", request: " << request_stream_.str();
    bool ret = session_->send_data(request_stream_.str());
    return ret;
}

bool FibpHttpClient::get_http_response(http::response_t& http_rsp)
{
    can_retry_ = true;
    // parse the HTTP response to json response data.
    session_->prepare_timeout(session_->read_to_);

    next_rsp_.clear();
    boost::system::error_code ec;
    bool ret = rsp_parser_.parse_response(ec);
    session_->clear_timeout();

    http_rsp.swap(next_rsp_);
    if (!http_rsp.keep_alive_ || ec == boost::asio::error::eof
        || ec == boost::asio::error::operation_aborted)
    {
        LOG(INFO) << "http session closed.";
        session_->shutdown(true);
    }

    if (ret && http_rsp.code_ == http::OK)
    {
        return true;
    }
    else
    {
        LOG(INFO) << "get http response failed." << http_rsp;
        if (http_rsp.code_ == http::BAD_REQUEST ||
            http_rsp.code_ == http::NOT_FOUND)
        {
            can_retry_ = false;
        }
        if (ec == boost::asio::error::operation_aborted)
        {
            http_rsp.status_message_ = TIMEOUT_ERR;
        }
    }
    return false;
}

bool FibpHttpClient::send_request(
    const std::string& path,
    http::method method,
    const std::string& reqdata,
    int timeout_ms)
{
    // generate the request data for HTTP.
    std::string query;
    std::string path_without_query = path;
    std::size_t pos = path.find("?");
    if (pos != std::string::npos)
    {
        path_without_query = path.substr(0, pos);
        query = path.substr(pos + 1);
    }

    //LOG(INFO) << "http path: " << path_without_query << ", query:" << query <<
    //    ", data:" << reqdata;

    http::request_t http_request;
    http_request.method_ = method;
    http_request.path_ = path_without_query;
    http_request.query_ = query;
    http_request.keep_alive_ = true;
    http_request.body_ = reqdata;

    bool ret = send_http_request(http_request, timeout_ms);
    return ret;
}

bool FibpHttpClient::get_response(std::string& rsp)
{
    can_retry_ = true;
    // parse the HTTP response to json response data.
    session_->prepare_timeout(session_->read_to_);
    next_rsp_.clear();
    boost::system::error_code ec;
    bool ret = rsp_parser_.parse_response(ec);
    session_->clear_timeout();
    if (!next_rsp_.keep_alive_ || ec == boost::asio::error::eof
        || ec == boost::asio::error::operation_aborted)
    {
        LOG(INFO) << "http session closed.";
        session_->shutdown(true);
    }
    if (ret && next_rsp_.code_ == http::OK)
    {
        rsp = next_rsp_.body_;
        return true;
    }
    else
    {
        LOG(INFO) << "get http response failed." << ec.message() << ", " << next_rsp_;
        if (next_rsp_.code_ == http::BAD_REQUEST ||
            next_rsp_.code_ == http::NOT_FOUND)
        {
            can_retry_ = false;
        }
        rsp = next_rsp_.status_message_;
        if (ec == boost::asio::error::operation_aborted)
        {
            rsp = TIMEOUT_ERR;
        }
    }
    return false;
}

FibpRpcClient::FibpRpcClient(boost::asio::io_service& io_service, const std::string& host, const std::string& port)
    : FibpClientBase(io_service, host, port), fid_(0)
{
    rpc_pac_.reset(new msgpack::unpacker());
    rpc_buf_.reset(new msgpack::sbuffer());
}

FibpRpcClient::~FibpRpcClient()
{
    session_->shutdown(true);
}

FibpClientFuturePtr FibpRpcClient::send_request(
    const std::string& path,
    const std::string& reqdata,
    int timeout_ms)
{
    session_->set_timeout(timeout_ms*2, timeout_ms);
    // path is used as method name in the rpc call.
    uint32_t msgid = ++fid_;

    msg_request rpc_req;
    rpc_req.msgid = msgid;
    rpc_req.method = path;
    rpc_req.packed_param = reqdata;
    rpc_buf_->clear();
    msgpack::packer<msgpack::sbuffer> pk(rpc_buf_.get());
    rpc_req.pack(pk);
    std::string packed_senddata;
    packed_senddata.assign(rpc_buf_->data(), rpc_buf_->size());

    //LOG(INFO) << "begin send rpc request : " << msgid;
    //LOG(INFO) << "rpc packed data: " << packed_senddata_.size();
    //for(std::size_t i = 0; i < packed_senddata_.size(); ++i)
    //{
    //    printf(" %02x", (unsigned char)(packed_senddata_[i]));
    //}
    //printf("\n");
    FibpClientFuturePtr f(new FibpClientFuture(session_->get_io_service(), msgid, timeout_ms));
    future_list_[msgid] = f;
    bool ret = session_->send_data(packed_senddata);
    if (!ret)
    {
        future_list_.erase(msgid);
        return FibpClientFuturePtr();
    }

    if (!reading_fiber_)
    {
        reading_fiber_.reset(new boost::fibers::fiber(boost::bind(&FibpRpcClient::handle_read, this)));
        reading_fiber_->detach();
    }
    //LOG(INFO) << "end send rpc request : " << msgid;
    return f;
}

void FibpRpcClient::handle_read()
{
    std::string rsp;
    LOG(INFO) << "handle read in fiber: " << boost::this_fiber::get_id();
    while(true)
    {
        if (rpc_pac_->execute())
        {
            msgpack::object msg = rpc_pac_->data();
            msgpack::auto_zone z(rpc_pac_->release_zone());
            rpc_pac_->reset();

            msg_response rpc_rsp;
            bool ret = false;
            try
            {
                msg.convert(&rpc_rsp);
                if (!rpc_rsp.err.is_nil())
                {
                    rsp.clear();
                    rpc_rsp.err.convert(&rsp);
                    ret = false;
                }
                else
                {
                    rpc_buf_->clear();
                    msgpack::packer<msgpack::sbuffer> pk(rpc_buf_.get());
                    pk.pack(rpc_rsp.result);
                    rsp.assign(rpc_buf_->data(), rpc_buf_->size());
                    ret = true;
                }
            }
            catch(const std::exception& e)
            {
                rsp = e.what();
                goto rpc_fail;
            }
            set_response(rpc_rsp.msgid, rsp, ret, false);

            if (rpc_pac_->nonparsed_size() > 0)
            {
                continue;
            }
        }

        bs::error_code ec;
        rpc_pac_->reserve_buffer(1024*32);
        std::size_t bytes_read = 0;
        //LOG(INFO) << "begin read rpc data.";
        bytes_read = session_->socket_.async_read_some(
            ba::mutable_buffers_1(rpc_pac_->buffer(), rpc_pac_->buffer_capacity()),
            boost::fibers::asio::yield[ec]);
        if (ec)
        {
            rsp = ec.message();
            goto rpc_fail;
        }
        //LOG(INFO) << "read rpc data : " << bytes_read;

        rpc_pac_->buffer_consumed(bytes_read);

        //msgpack::unpacked result;
        //while(rpc_pac_->next(&result))
        //{
        //    msg_response rpc_rsp;
        //    bool ret = false;
        //    try
        //    {
        //        msgpack::object obj = result.get();
        //        obj.convert(&rpc_rsp);
        //        if (!rpc_rsp.err.is_nil())
        //        {
        //            rsp.clear();
        //            rpc_rsp.err.convert(&rsp);
        //            ret = false;
        //        }
        //        else
        //        {
        //            rpc_buf_->clear();
        //            msgpack::packer<msgpack::sbuffer> pk(rpc_buf_.get());
        //            pk.pack(rpc_rsp.result);
        //            rsp.assign(rpc_buf_->data(), rpc_buf_->size());
        //            ret = true;
        //        }
        //    }
        //    catch(const std::exception& e)
        //    {
        //        LOG(WARNING) << "got rpc response exception: " << e.what();
        //        rsp = e.what();
        //        goto rpc_fail;
        //    }
        //    set_response(rpc_rsp.msgid, rsp, ret, false);
        //}
        if (rpc_pac_->message_size() > 10*1024*1024)
        {
            LOG(ERROR) << "Rpc message response too large." << rpc_pac_->message_size();
            rsp = SERVER_RSP_TOO_LARGE_ERR;
            goto rpc_fail;
        }
    }

rpc_fail:
    rpc_pac_.reset(new msgpack::unpacker());
    session_->shutdown(true);
    LOG(INFO) << "read error :" << rsp;
    set_all_error(rsp);
    reading_fiber_.reset();
}

void FibpRpcClient::set_response(uint32_t msgid, const std::string& rsp, bool is_success, bool can_retry)
{
    FibpClientFuturePtr f = future_list_[msgid];
    if (f)
    {
        f->set_result(rsp, is_success, can_retry);
    }
    future_list_.erase(msgid);
    //LOG(INFO) << "future ready: " << msgid << " in fiber:" << boost::this_fiber::get_id();
}

void FibpRpcClient::set_all_error(const std::string& errinfo)
{
    for(FutureListT::iterator it = future_list_.begin();
        it != future_list_.end(); ++it)
    {
        if (it->second)
            it->second->set_result(errinfo, false, true);
    }
    future_list_.clear();
}

bool FibpRpcClient::get_response(FibpClientFuturePtr f, std::string& rsp, bool& can_retry)
{
    return f->getRsp(rsp, can_retry);

    //can_retry_ = true;
    rsp.clear();
    bs::error_code ec;
    while(true)
    {
        rpc_pac_->reserve_buffer(1024*32);
        std::size_t bytes_read = 0;
        ec = session_->async_read_some(
            ba::mutable_buffers_1(rpc_pac_->buffer(), rpc_pac_->buffer_capacity()),
            rpc_pac_->buffer_capacity(), bytes_read);
        if (ec)
        {
            rsp = ec.message();
            LOG(INFO) << ec.message();
            goto rpc_fail_get;
        }
        rpc_pac_->buffer_consumed(bytes_read);
        msgpack::unpacked result;
        msg_response rpc_rsp;
        while(rpc_pac_->next(&result))
        {
            try
            {
                msgpack::object obj = result.get();
                obj.convert(&rpc_rsp);
                if (!rpc_rsp.err.is_nil())
                {
                    rpc_rsp.err.convert(&rsp);
                    //can_retry_ = false;
                    return false;
                }
                else
                {
                    rpc_buf_->clear();
                    msgpack::packer<msgpack::sbuffer> pk(rpc_buf_.get());
                    pk.pack(rpc_rsp.result);
                    rsp.assign(rpc_buf_->data(), rpc_buf_->size());
                }
            }
            catch(const std::exception& e)
            {
                LOG(WARNING) << "got rpc response exception: " << e.what();
                goto rpc_fail_get;
            }
            return true;
        }
        if (rpc_pac_->message_size() > 10*1024*1024)
        {
            LOG(ERROR) << "Rpc message response too large." << rpc_pac_->message_size();
            rsp = SERVER_RSP_TOO_LARGE_ERR;
            goto rpc_fail_get;
        }
    }
rpc_fail_get:
    rpc_pac_.reset(new msgpack::unpacker());
    session_->shutdown(true);
    return false;
}

FibpRawClient::FibpRawClient(boost::asio::io_service& io_service, const std::string& host, const std::string& port)
    : FibpClientBase(io_service, host, port), fid_(0)
{
}

FibpRawClient::~FibpRawClient()
{
    session_->shutdown(true);
}

bool FibpRawClient::get_response(FibpClientFuturePtr f, std::string& rsp, bool& can_retry)
{
    return f->getRsp(rsp, can_retry);

    rsp.clear();
    bool ret = readRpsHeader(rsp);
    return ret;
}

void FibpRawClient::handle_read()
{
    std::string rsp;
    while(true)
    {
        bs::error_code ec;
        static const std::size_t HEAD_SIZE = 2*sizeof(uint32_t);
        char header[HEAD_SIZE];
        ba::async_read(session_->socket_, ba::mutable_buffers_1(header, HEAD_SIZE),
            boost::fibers::asio::yield[ec]);
        if (ec)
        {
            rsp = ec.message();
            break;
        }
        uint32_t seq, len;
        memcpy(&seq, header, sizeof(seq));
        memcpy(&len, header + sizeof(seq), sizeof(len));
        seq = ntohl(seq);
        len = ntohl(len);

        //LOG(INFO) << "received seq: " << seq << ", body len: " << len;
        rsp.resize(len);

        ba::async_read(session_->socket_, boost::asio::mutable_buffers_1(&rsp[0], rsp.size()),
            boost::fibers::asio::yield[ec]);
        if (ec)
        {
            rsp = ec.message();
            break;
        }
        set_response(seq, rsp, true, false);
    }
    set_all_error(rsp);
    reading_fiber_.reset();
}

void FibpRawClient::set_response(uint32_t msgid, const std::string& rsp, bool is_success, bool can_retry)
{
    FibpClientFuturePtr f = future_list_[msgid];
    if (f)
    {
        f->set_result(rsp, is_success, can_retry);
    }
    future_list_.erase(msgid);
}

void FibpRawClient::set_all_error(const std::string& errinfo)
{
    for(FutureListT::iterator it = future_list_.begin();
        it != future_list_.end(); ++it)
    {
        if (it->second)
        {
            it->second->set_result(errinfo, false, true);
        }
    }
    future_list_.clear();
}

bool FibpRawClient::readRpsHeader(std::string& rsp)
{
    bs::error_code ec;
    static const std::size_t HEAD_SIZE = 2*sizeof(uint32_t);
    char header[HEAD_SIZE];
    ec = session_->async_read(ba::mutable_buffers_1(header, HEAD_SIZE), HEAD_SIZE);
    return afterReadRspHeader(ec, header, rsp);
}

bool FibpRawClient::afterReadRspHeader(const bs::error_code& ec,
    const char* header, std::string& rsp)
{
    if (ec)
    {
        //LOG(INFO) << "get header failed in raw client.";
        if (ec == boost::asio::error::operation_aborted)
        {
            rsp = TIMEOUT_ERR;
        }
        return false;
    }
    uint32_t seq, len;
    memcpy(&seq, header, sizeof(seq));
    memcpy(&len, header + sizeof(seq), sizeof(len));
    seq = ntohl(seq);
    len = ntohl(len);

    //LOG(INFO) << "received seq: " << seq << ", body len: " << len;
    rsp.resize(len);

    return readRspBody(rsp);
}

bool FibpRawClient::readRspBody(std::string& rsp)
{
    bs::error_code ec;
    ec = session_->async_read(boost::asio::mutable_buffers_1(&rsp[0], rsp.size()), rsp.size());
    return afterReadRspBody(ec, rsp);
}

bool FibpRawClient::afterReadRspBody(const bs::error_code& ec,
    std::string& rsp)
{
    if (ec)
    {
        //LOG(INFO) << "get body failed in raw client.";
        rsp.clear();
        if (ec == boost::asio::error::operation_aborted)
        {
            rsp = TIMEOUT_ERR;
        }
        return false;
    }
    return true;
}

FibpClientFuturePtr FibpRawClient::send_request(
    const std::string& path,
    const std::string& reqdata,
    int timeout_ms)
{
    session_->set_timeout(timeout_ms*2, timeout_ms);
    uint32_t fid = ++fid_;
    uint32_t len = reqdata.size();

    std::string packed_senddata;
    uint32_t fid_n = htonl(fid);
    len = htonl(len);
    packed_senddata.resize(sizeof(fid_n) + sizeof(len) + reqdata.size());
    memcpy(&packed_senddata[0], &fid_n, sizeof(fid_n));
    memcpy(&packed_senddata[0] + sizeof(fid_n), &len, sizeof(len));
    memcpy(&packed_senddata[0] + sizeof(fid_n) + sizeof(len), &reqdata[0], reqdata.size());
    FibpClientFuturePtr f(new FibpClientFuture(session_->get_io_service(), fid, timeout_ms));
    future_list_[fid] = f;
    bool ret = session_->send_data(packed_senddata);
    if (!ret)
    {
        future_list_.erase(fid);
        return FibpClientFuturePtr();
    }
    if (!reading_fiber_)
    {
        reading_fiber_.reset(new boost::fibers::fiber(boost::bind(&FibpRawClient::handle_read, this)));
        reading_fiber_->detach();
    }
    return f;
}

}
