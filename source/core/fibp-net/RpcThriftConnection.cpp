#include "RpcThriftConnection.h"

namespace fibp
{

RpcThriftConnection::RpcThriftConnection()
{
}

RpcThriftConnection::~RpcThriftConnection()
{
}

void RpcThriftConnection::test()
{
    //boost::shared_ptr<TTransport> mem(new TMemoryBuffer());
    //boost::shared_ptr<TTransport> transport(new TBufferedTransport(mem));
    //boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(transport));
    //
    //boost::shared_ptr<TMemoryBuffer> mem(new TMemoryBuffer());
    //boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(mem));

    //obj.write(protocol.get());

    //std::string packed_data = mem->getBufferAsString();

    //boost::shared_ptr<TMemoryBuffer> mem(new TMemoryBuffer(packed_data.data(), packed_data.size()));
    //boost::shared_ptr<TProtocol> protocol(new TBinaryProtocol(mem));

    //obj.read(protocol.get());
    //readThriftObjFromBuffer(packed_data.data(), packed_data.size());
}

}
