INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}/core
  ${izenelib_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${Glog_INCLUDE_DIRS}
  )

SET(libs
    ${Boost_LIBRARIES}
    ${Glog_LIBRARIES}
    )

ADD_EXECUTABLE(t_stress_test
    t_stress_test.cpp
    )

ADD_EXECUTABLE(t_log_test
    t_log_test.cpp
    )

ADD_EXECUTABLE(t_rpc_test
    t_rpc_test.cpp
    )

ADD_EXECUTABLE(t_fiber_test
    test_fiber.cpp
    )

TARGET_LINK_LIBRARIES(t_stress_test ${libs})
TARGET_LINK_LIBRARIES(t_log_test ${libs} fibp_fiber fibp_log_manager fibp_fiber_server
    fibp_forward_manager ${izenelib_LIBRARIES}
    )

TARGET_LINK_LIBRARIES(t_rpc_test ${libs} ${izenelib_LIBRARIES})
TARGET_LINK_LIBRARIES(t_fiber_test fibp_fiber ${libs} -lboost_unit_test_framework )
  
SET_TARGET_PROPERTIES(t_stress_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTOR ${FIBP_ROOT}/testbin
    )
SET_TARGET_PROPERTIES(t_log_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTOR ${FIBP_ROOT}/testbin
    )
SET_TARGET_PROPERTIES(t_rpc_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTOR ${FIBP_ROOT}/testbin
    )
SET_TARGET_PROPERTIES(t_fiber_test PROPERTIES
    RUNTIME_OUTPUT_DIRECTOR ${FIBP_ROOT}/testbin
    )
