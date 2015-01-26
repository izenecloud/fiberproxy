#ifndef LOG_SERVER_REQUEST_DATA_H_
#define LOG_SERVER_REQUEST_DATA_H_

#include <3rdparty/msgpack/msgpack.hpp>

#include <string>
#include <vector>
#include <map>

namespace fibp
{

inline std::ostream& operator<<(std::ostream& os, uint128_t uint128)
{
    os << hex << uint64_t(uint128>>64) << uint64_t(uint128) << dec;
    return os;
}

struct LogServerRequestData
{
};

}

#endif /* LOG_SERVER_REQUEST_DATA_H_ */
