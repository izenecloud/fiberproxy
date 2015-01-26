/**
 * @file core/common/Keys.cpp
 * @author Ian Yang
 * @date Created <2010-06-10 15:29:55>
 */
#include "Keys.h"


namespace fibp {
namespace driver {

#define FIBP_DRIVER_KEYS_DEF(z,i,l) \
    const std::string Keys::BOOST_PP_SEQ_ELEM(i,l) = \
        BOOST_PP_STRINGIZE(BOOST_PP_SEQ_ELEM(i,l));

BOOST_PP_REPEAT(
    BOOST_PP_SEQ_SIZE(FIBP_DRIVER_KEYS),
    FIBP_DRIVER_KEYS_DEF,
    FIBP_DRIVER_KEYS
)

#undef FIBP_DRIVER_KEYS_DEF
}} // namespace fibp::driver
