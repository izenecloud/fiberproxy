INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/core/
  ${CMAKE_SOURCE_DIR}/core/boost-fiber/
  ${CMAKE_SOURCE_DIR}

  ${izenelib_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${Glog_INCLUDE_DIRS}
)

FILE(GLOB_RECURSE common_SRC "common/*.cpp")
FILE(GLOB_RECURSE log_SRC "log-manager/*.cpp")
#FILE(GLOB_RECURSE node_SRC "node-manager/*.cpp")
#FILE(GLOB config_SRC "configuration-manager/*.cpp")
FILE(GLOB fibp_net_SRC "fibp-net/*.cpp")
FILE(GLOB_RECURSE fibp_fiber_SRC "boost-fiber/*.cpp")
FILE(GLOB_RECURSE fibp_fiber_server_SRC "fiber-server/*.cpp")
FILE(GLOB_RECURSE fibp_forward_manager_SRC "forward-manager/*.cpp")

ADD_DEFINITIONS("-fno-strict-aliasing")
ADD_LIBRARY(fibp_common ${common_SRC})
ADD_LIBRARY(fibp_log_manager ${log_SRC})
#ADD_LIBRARY(fibp_node_manager ${node_SRC})
#ADD_LIBRARY(fibp_config_manager ${config_SRC})
ADD_LIBRARY(fibp_net ${fibp_net_SRC})
ADD_LIBRARY(fibp_fiber ${fibp_fiber_SRC})
ADD_LIBRARY(fibp_fiber_server ${fibp_fiber_server_SRC})
ADD_LIBRARY(fibp_forward_manager ${fibp_forward_manager_SRC})

INSTALL(TARGETS
    fibp_common
    fibp_log_manager
    #fibp_node_manager
    #fibp_config_manager
    fibp_net
    fibp_fiber
    fibp_fiber_server
    fibp_forward_manager
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    COMPONENT fibp_libraries)
