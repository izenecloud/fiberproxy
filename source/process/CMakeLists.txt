INCLUDE_DIRECTORIES(
  ${CMAKE_SOURCE_DIR}
  ${CMAKE_SOURCE_DIR}/core/
  ${CMAKE_SOURCE_DIR}/core/boost-fiber/
  ${CMAKE_CURRENT_SOURCE_DIR}

  ${izenelib_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${Glog_INCLUDE_DIRS}
  ${XML2_INCLUDE_DIR}
  ${SQLITE3_INCLUDE_DIR}
  )

MESSAGE(STATUE "include path: ${INCLUDE_DIRECTORIES}")

FILE(GLOB_RECURSE common_SRC "common/*.cpp")
FILE(GLOB controller_SRC "controllers/*.cpp")


ADD_DEFINITIONS("-fno-strict-aliasing")

FILE(GLOB fibpserver_process_SRC
    "FibpServer*.cpp"
  ${common_SRC}
  ${controller_SRC}
  )

ADD_DEFINITIONS("-fno-strict-aliasing")
ADD_EXECUTABLE(FibpServerProcess ${fibpserver_process_SRC})

TARGET_LINK_LIBRARIES(FibpServerProcess

  fibp_log_manager
  fibp_common
  fibp_net
  fibp_fiber
  fibp_fiber_server
  fibp_forward_manager

  ${izenelib_LIBRARIES}

  #external
  ${XML2_LIBRARIES}
  ${Boost_LIBRARIES}
  ${Glog_LIBRARIES}
  ${SQLITE3_LIBRARIES}
  ${MYSQL_LIBRARIES}
  ${LibCURL_LIBRARIES}
  pthread
  )

INSTALL(TARGETS
  FibpServerProcess
  RUNTIME DESTINATION bin
  COMPONENT fibp_apps)
