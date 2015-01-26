#ifndef _LOG_SERVER_REQUEST_H_
#define _LOG_SERVER_REQUEST_H_

#include "LogServerRequestData.h"

namespace fibp
{

class LogServerRequest
{
public:
    typedef std::string method_t;

    /// add method here
    enum METHOD
    {
        METHOD_TEST = 0,

        COUNT_OF_METHODS
    };

    static const method_t method_names[COUNT_OF_METHODS];

    METHOD method_;

public:
    LogServerRequest(const METHOD& method) : method_(method) {}
    virtual ~LogServerRequest() {}
};

template <typename RequestDataT>
class LogRequestRequestT : public LogServerRequest
{
public:
    LogRequestRequestT(METHOD method)
        : LogServerRequest(method)
    {
    }

    RequestDataT param_;

    MSGPACK_DEFINE(param_)
};

}

#endif /* _LOG_SERVER_REQUEST_H_ */
