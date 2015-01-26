#ifndef FIBP_DRIVER_KEYS_H
#define FIBP_DRIVER_KEYS_H
#include <boost/preprocessor.hpp>
#include <util/driver/Keys.h>
#include <string>

#include "Keys.inl"

namespace fibp {
namespace driver {

#define FIBP_DRIVER_KEYS_DECL(z,i,l) \
    static const std::string BOOST_PP_SEQ_ELEM(i, l);

struct Keys : public ::izenelib::driver::Keys
{
    BOOST_PP_REPEAT(
        BOOST_PP_SEQ_SIZE(FIBP_DRIVER_KEYS),
        FIBP_DRIVER_KEYS_DECL,
        FIBP_DRIVER_KEYS
    )
};

#undef FIBP_DRIVER_KEYS_DECL
}} // namespace fibp::driver

#endif // 
