#include <stdlib.h>
#include <boost/thread.hpp>
#include <boost/asio.hpp>
#include <string>
#include <glog/logging.h>

using boost::asio::ip::tcp;
static std::string dest_ip;
static std::string dest_port;
boost::atomic<uint32_t> g_seq(1);

void send_raw_request()
{
    boost::asio::io_service io_service;

    tcp::resolver resolver(io_service);
    tcp::resolver::query query(dest_ip, dest_port);
    tcp::resolver::iterator endpoint_iterator = resolver.resolve(query);
    tcp::resolver::iterator end;

    tcp::socket socket(io_service);
    boost::system::error_code error = boost::asio::error::host_not_found;
    while (error && endpoint_iterator != end)
    {
      socket.close();
      socket.connect(*endpoint_iterator++, error);
    }
    if (error)
    {
        std::cerr << "failed to connect." << error.message() << std::endl;
        return;
    }

    uint32_t len = 0;
    std::string req_body;
    req_body = "{\
                \"header\":{\"controller\":\"commands\", \"action\":\"call_services_async\"},\
                \"call_api_list\":\
                [\
                {\
                    \"service_name\":\"local_test\",\
                        \"service_type\":2,\
                        \"service_req_data\":\"{}\"\
                },\
    {\
        \"service_name\":\"local_test\",\
            \"service_type\":0,\
            \"service_req_data\":\"{}\"\
    }]}";
    len = req_body.size();

    uint32_t seq = ++g_seq;
    std::string buf;
    buf.resize(sizeof(uint32_t)*2 + len);
    len = htonl(len);
    memcpy(&buf[0], &seq, sizeof(seq));
    memcpy(&buf[0] + sizeof(seq), &len, sizeof(len));
    memcpy(&buf[0] + sizeof(seq) + sizeof(len), &req_body[0], req_body.size());

    int test_times = 100000;
    int err_num = 0;
    while(--test_times > 0)
    {
        // Send the request.
        seq = ++g_seq;
        memcpy(&buf[0], &seq, sizeof(seq));
        boost::asio::write(socket, boost::asio::const_buffers_1(&buf[0], buf.size()),
            error);
        if (error)
        {
            LOG(INFO) << error.message();
            return;
        }

        char header[sizeof(uint32_t)*2];
        // Read the response headers, which are terminated by a blank line.
        boost::asio::read(socket, boost::asio::mutable_buffers_1(header, sizeof(uint32_t)*2),
            error);
        if (error)
        {
            ++err_num;
            continue;
        }
        uint32_t len_received;
        uint32_t seq_received;
        memcpy(&seq_received, header, sizeof(seq_received));
        memcpy(&len_received, header + sizeof(uint32_t), sizeof(uint32_t));
        len_received = ntohl(len_received);

        char *body = new char[len_received + 1];
        boost::asio::read(socket, boost::asio::mutable_buffers_1(body, len_received),
            error);

        body[len_received] = '\0';
        if (test_times == 99999)
            LOG(INFO) << body;
        delete[] body;

        if (error || seq != seq_received)
        {
            ++err_num;
            continue;
        }

        if (test_times % 1000 == 0)
            LOG(INFO) << "finished test: " << test_times;
    }
    LOG(INFO) << "finished all, error number: " << err_num;
    socket.close();
}

int main(int argc, char* argv[])
{
    if (argc < 3)
    {
        LOG(INFO) << "ip port needed.";
        return -1;
    }
    dest_ip = argv[1];
    dest_port = argv[2];
    boost::thread_group thread_list;
    std::size_t num = 100;
    for(std::size_t i = 0; i < num; ++i)
    {
        thread_list.create_thread(boost::bind(&send_raw_request));
    }
    thread_list.join_all();
    return 0;
}
