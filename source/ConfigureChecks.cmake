##################################################
# Headers
#####
INCLUDE(CheckIncludeFile)

# int types
CHECK_INCLUDE_FILE(inttypes.h HAVE_INTTYPES_H)
CHECK_INCLUDE_FILE(stdint.h HAVE_STDINT_H)
CHECK_INCLUDE_FILE(sys/types.h HAVE_SYS_TYPES_H)
CHECK_INCLUDE_FILE(sys/stat.h HAVE_SYS_STAT_H)
CHECK_INCLUDE_FILE(stddef.h HAVE_STDDEF_H)

# signal.h
CHECK_INCLUDE_FILE(signal.h HAVE_SIGNAL_H)

# ext hash
CHECK_INCLUDE_FILE(ext/hash_map HAVE_EXT_HASH_MAP)

##################################################
# Our Proprietary Libraries
#####

FIND_PACKAGE(izenelib REQUIRED COMPONENTS
  izene_util
  febird
  ticpp
  jemalloc
  json
  aggregator
  msgpack
  zookeeper
  compressor
  )
##################################################
# Other Libraries
#####

FIND_PACKAGE(XML2 REQUIRED)
FIND_PACKAGE(Threads REQUIRED)

SET(Boost_ADDITIONAL_VERSIONS 1.56)
FIND_PACKAGE(Boost 1.56 REQUIRED
  COMPONENTS
  system
  program_options
  thread
  regex
  date_time
  serialization
  filesystem
  unit_test_framework
  iostreams
  context
  coroutine
  chrono
  #fiber
  atomic
  )

FIND_PACKAGE(Glog REQUIRED)
##################################################
# Driver Docs
#####
GET_FILENAME_COMPONENT(FIBP_PARENT_DIR "${FIBP_ROOT}" PATH)
SET(FIBP_DRIVER_DOCS_ROOT "${FIBP_PARENT_DIR}/fibp-driver-docs/")

##################################################
# Doxygen
#####
FIND_PACKAGE(Doxygen)
IF(DOXYGEN_DOT_EXECUTABLE)
  OPTION(USE_DOT "use dot in doxygen?" FLASE)
ENDIF(DOXYGEN_DOT_EXECUTABLE)

SET(USE_DOT_YESNO NO)
IF(USE_DOT)
  SET(USE_DOT_YESNO YES)
ENDIF(USE_DOT)

set(SYS_LIBS
  m rt dl z )

