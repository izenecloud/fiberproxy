MESSAGE(STATUS "****** Starting to add libraries ******")
SET(CPACK_PACKAGE_VERSION_MAJOR 0 )
SET(CPACK_PACKAGE_VERSION_MINOR 6 )

####################################################
# Macro for getting version number out of a .so file
####
SET(THREE_PART_VERSION_REGEX ".+\\.so\\.[0-9]+\\.[0-9]+\\.[0-9]+")
SET(TWO_PART_VERSION_REGEX ".+\\.so\\.[0-9]+\\.[0-9]+")
SET(ONE_PART_VERSION_REGEX ".+\\.so\\.[0-9]+")
SET(NONE_PART_VERSION_REGEX ".+\\.so")

# Breaks up a string in the form n1.n2.n3 into three parts and stores
# them in major, minor, and patch.  version should be a value, not a
# variable, while major, minor and patch should be variables.
MACRO(THREE_PART_VERSION_TO_VARS version major minor patch)
    #MESSAGE(STATUS "THREE_PART_VERSION_TO_VARS >>>> ${version}")
    IF(${version} MATCHES ${THREE_PART_VERSION_REGEX})
        STRING(REGEX REPLACE ".+\\.so\\.([0-9]+)\\.[0-9]+\\.[0-9]+" "\\1"
            ${major} "${version}")
        STRING(REGEX REPLACE ".+\\.so\\.[0-9]+\\.([0-9]+)\\.[0-9]+" "\\1"
            ${minor} "${version}")
        STRING(REGEX REPLACE ".+\\.so\\.[0-9]+\\.[0-9]+\\.([0-9]+)" "\\1"
            ${patch} "${version}")
    ELSEIF(${version} MATCHES ${TWO_PART_VERSION_REGEX})
        STRING(REGEX REPLACE ".+\\.so\\.([0-9]+)\\.[0-9]+" "\\1"
            ${major} "${version}")
        STRING(REGEX REPLACE ".+\\.so\\.[0-9]+\\.([0-9]+)" "\\1"
            ${minor} "${version}")
        SET(${patch} "0")
    ELSEIF(${version} MATCHES ${ONE_PART_VERSION_REGEX})
        STRING(REGEX REPLACE ".+\\.so\\.([0-9]+)" "\\1"
            ${major} "${version}")
        SET(${minor} "0")
        SET(${patch} "0")
    ELSEIF(${version} MATCHES ${NONE_PART_VERSION_REGEX})
        SET(${major} "0")
        SET(${minor} "0")
        SET(${patch} "0")
    ELSE(${version} MATCHES ${THREE_PART_VERSION_REGEX})
        MESSAGE(STATUS "MACRO(THREE_PART_VERSION_TO_VARS ${version} ${major} ${minor} ${patch}")
        MESSAGE(FATAL_ERROR "Problem parsing version string, I can't parse it properly.")
    ENDIF(${version} MATCHES
        ${THREE_PART_VERSION_REGEX})
ENDMACRO(THREE_PART_VERSION_TO_VARS)

##################################################
# Macro for copying third party libraries (boost, tokyo cabinet, glog)
####
MACRO(INSTALL_RELATED_LIBRARIES)
MESSAGE(STATUS "Packing >>>>>> ${ARGV}")
FOREACH(thirdlib ${ARGV})
    #MESSAGE(STATUS ">>>>>> ${thirdlib}")
    IF( ${thirdlib} MATCHES ".+\\.so" OR ${thirdlib} MATCHES ".+\\.so\\..+" )

        get_filename_component(libfilename ${thirdlib} NAME)
        get_filename_component(libpath ${thirdlib} PATH)

        # add the library files that exists
        SET(filestocopy ${thirdlib})

        EXECUTE_PROCESS( COMMAND "readlink"
            ${thirdlib} OUTPUT_VARIABLE oriname )

        IF(oriname)
            STRING(STRIP ${oriname} oriname)

            MESSAGE(STATUS "NAME: ${libfilename}")
            MESSAGE(STATUS "ORINAME: ${oriname}")
            MESSAGE(STATUS "PATH: ${libpath}")

            THREE_PART_VERSION_TO_VARS(${oriname} major_vers minor_vers patch_vers)
            MESSAGE("version = ${major_vers}%${minor_vers}%${patch_vers}")

            MESSAGE(STATUS "EXISTS: ${thirdlib}.${major_vers}")
            IF(EXISTS "${thirdlib}.${major_vers}")
                LIST(APPEND filestocopy "${thirdlib}.${major_vers}")
            ENDIF(EXISTS "${thirdlib}.${major_vers}")

            MESSAGE(STATUS "EXISTS: ${libpath}/${oriname}" )
            IF(EXISTS "${libpath}/${oriname}" )
                LIST(APPEND filestocopy "${libpath}/${oriname}")
            ENDIF(EXISTS "${libpath}/${oriname}" )
        ENDIF(oriname)

        MESSAGE(STATUS "@@@@ ${filestocopy}")

        INSTALL(PROGRAMS ${filestocopy}
            DESTINATION lib-thirdparty
            COMPONENT  fibp_packings
            )
    ENDIF( ${thirdlib} MATCHES ".+\\.so" OR ${thirdlib} MATCHES ".+\\.so\\..+" )
ENDFOREACH(thirdlib)
ENDMACRO(INSTALL_RELATED_LIBRARIES)


##################################################
# Installing third party library files
####
SET(ENV_ONLY_PACKAGE_FIBP $ENV{ONLY_PACKAGE_FIBP})
IF(NOT ENV_ONLY_PACKAGE_FIBP)
  INSTALL_RELATED_LIBRARIES(${Boost_LIBRARIES})
  INSTALL_RELATED_LIBRARIES(${izenelib_LIBRARIES})
  INSTALL_RELATED_LIBRARIES(${XML2_LIBRARIES})
  INSTALL_RELATED_LIBRARIES(${Glog_LIBRARIES})

  INSTALL( DIRECTORY "${CMAKE_SOURCE_DIR}/../bin/config"
    DESTINATION "."
    COMPONENT fibp_packings
    REGEX ".xml.in" EXCLUDE
    PATTERN "example.xml" EXCLUDE
    )
  INSTALL( PROGRAMS
    ${CMAKE_SOURCE_DIR}/../bin/fibp-server
    ${CMAKE_SOURCE_DIR}/../bin/reg-fibp.sh
    ${CMAKE_SOURCE_DIR}/../bin/reg-fibp.json
    DESTINATION "bin"
    COMPONENT fibp_packings
    )
#INSTALL( DIRECTORY "${CMAKE_SOURCE_DIR}/../package/resource"
#    DESTINATION "."
#    COMPONENT fibp_packings
#    REGEX ".gitignore" EXCLUDE
#    )

ENDIF(NOT ENV_ONLY_PACKAGE_FIBP)

##################################################
# Settings for CPack
####
SET(CPACK_PACKAGE_NAME "FIBP x86_64 GNU/Linux" )

IF( USE_MF_LIGHT )

SET(CPACK_PACKAGE_VENDOR "IZENEsoft" )
SET(CPACK_PACKAGE_VERSION "beta")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "FIBP Trial" )
SET(CPACK_PACKAGE_FILE_NAME "FIBP-${CPACK_PACKAGE_VERSION}-FibpProcess-x86_64-Linux-GCC4.1.2" )
SET(CPACK_PACKAGE_FILE_NAME "fibp" )

ELSE( USE_MF_LIGHT )

SET(CPACK_PACKAGE_VENDOR "Wisenut" )
SET(CPACK_PACKAGE_VERSION "alpha")
SET(CPACK_PACKAGE_DESCRIPTION_SUMMARY "fibp QC candidate" )
SET(CPACK_PACKAGE_FILE_NAME "FIBP-${CPACK_PACKAGE_VERSION}-x86_64-Linux-GCC4.1.2" )
SET(CPACK_PACKAGE_FILE_NAME "fibp-server" )

ENDIF( USE_MF_LIGHT )

SET(CPACK_GENERATOR "TGZ")
SET(CPACK_SOURCE_GENERATOR "TGZ")

IF(ENV_ONLY_PACKAGE_FIBP)
  SET(CPACK_COMPONENTS_ALL fibp_libraries fibp_apps)
ELSE(ENV_ONLY_PACKAGE_FIBP)
  SET(CPACK_COMPONENTS_ALL
    fibp_libraries
    fibp_apps
    fibp_packings
    )
ENDIF(ENV_ONLY_PACKAGE_FIBP)

INCLUDE(UseCPack)
