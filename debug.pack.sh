#!/bin/bash

## Confirm your repository path first
FIBP_DIR_NAME=${PWD##*/}
cd ..
CODEBASE="$(pwd)"
cd $FIBP_DIR_NAME
FIBP_Package="fibp-server"

today=`date +%Y.%m.%d`
#for compile_version in license debug
for compile_version in  debug
do
    ## ------------------------- CHECK COMMANDS ------------------------
    MAKE="make"
    CMAKE="cmake"
    GCC="gcc"
    COMMANDS_TO_CHECK="${MAKE} ${CMAKE} ${GCC}"

    # if any of the COMMANDS_TO_CHECK are not executable, then exit script
    OK="true"
    for c in ${COMMANDS_TO_CHECK} ; do
      CMD=`type -P $c 2>&1` ;
      if [ -z "${CMD}" ] ; then
        OK="false"
        echo "ERROR: unable to find command \"$c\" !"
      fi
    done
    if [ ${OK} != "true" ] ; then
      echo "Please add the above commands to your PATH or install missing programs and re-run the script ... exiting."
      exit 1
    fi

    # ------------------- CHECK REPOSITORY ----------------------
    export EXTRA_CMAKE_MODULES_DIRS="$CODEBASE/cmake"
    export IZENELIB="$CODEBASE/izenelib"
    export LIBXML2=/usr/include/libxml2
    export FIBP_DIR="$CODEBASE/$FIBP_DIR_NAME"
    export PATH=$PATH:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/usr/games
    export LD_LIBRARY_PATH=/usr/local/lib:$IZENELIB/lib:$ILPLIB/lib:$IDMLIB/lib:$IMLLIB/lib:$IZENECMA/lib:$IZENEJMA/lib:$IISE_ROOT/lib:$WISEKMA:$HOME/tfs_bin/lib:$TBLIB_ROOT/lib:$LD_LIBRARY_PATH

    dependencie=(cmake izenelib)

    element_count=${#dependencie[@]}
    index=0
    while [ "$index" -lt "$element_count" ]
    do
      cd $CODEBASE
      if [ ! -d  ${dependencie[$index]}  ];then
        echo "ERROR:${dependencie[$index]} doesn't exist."
        exit 1
      fi
      let "index = $index + 1"
    done

    ## --------------------- Start to compile -------------------
    echo -e "\nSync the repository ##$REPOSITORY##"
    cd $CODEBASE/cmake && git pull 2>/dev/null
    echo >/tmp/compile.$FIBP_DIR_NAME.log
    
    ## Sync and compile the libs
    for REPOSITORY in izenelib
    do
      echo -e "\nStart to compile ##$REPOSITORY##"
      cd $CODEBASE/$REPOSITORY && git pull 2>/dev/null
      cd build && rm -f CMakeCache.txt
      if [ -f $CODEBASE/$REPOSITORY/CMakeLists.txt ];then
        cmake -DEXTRA_CMAKE_MODULES_DIRS=$CODEBASE/cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo .. >>/tmp/compile.$FIBP_DIR_NAME.log 2>&1
      elif [ -f $CODEBASE/$REPOSITORY/source/CMakeLists.txt ];then
        cmake -DEXTRA_CMAKE_MODULES_DIRS=$CODEBASE/cmake -DCMAKE_BUILD_TYPE=RelWithDebInfo ../source >>/tmp/compile.$FIBP_DIR_NAME.log 2>&1
      else
        echo "ERROR: no CMakeLists.txt found in $CODEBASE!"
        exit 1
      fi
      make -j4 >>/tmp/compile.$FIBP_DIR_NAME.log 2>&1
    done

  if [ $compile_version = "debug" ]; then
    DCMAKE_BUILD_TYPE="RelWithDebInfo"
    DLICENSE_LEVEL=""
    echo -e "\nCompile Version: ## $compile_version ## ......"
    echo >/tmp/compile.$FIBP_DIR_NAME.log
    sleep 3

    echo -e "\nStart to compile ##$FIBP_DIR_NAME##"
    cd $FIBP_DIR && git pull 2>/dev/null
    cd build && rm -f CMakeCache.txt
    cmake -DCMAKE_BUILD_TYPE=$DCMAKE_BUILD_TYPE -DLICENSE_LEVEL=$DLICENSE_LEVEL ../source >>/tmp/compile.$FIBP_DIR_NAME.log 2>&1
    make -j4 >>/tmp/compile.$FIBP_DIR_NAME.log 2>&1
    make -j2 package >>/tmp/compile.$FIBP_DIR_NAME.log 2>&1

  else
    echo "ERROR: invalid compile version of $compile_version."
    exit 1
  fi

  ## --------------------- Verify and Repackage -------------------
  ## Check whether there is error in compiling log. 
  errorcount=$(cat /tmp/compile.$FIBP_DIR_NAME.log | grep -ic "^make.*error.*")

  ## If there isn't any error and tarball exists, continure to modify the tarball.
  if ([ $errorcount -eq "0" ] && [ -e "$FIBP_DIR/build/$FIBP_Package.tar.gz" ]) ; then
    mkdir -p /data/bak/$FIBP_Package/apps
	cd /data/bak/$FIBP_Package/apps
	## Modify the config.xml 
	rm -rf fibp $FIBP_Package
	tar xf $FIBP_DIR/build/$FIBP_Package.tar.gz
	mv $FIBP_Package fibp
	cd fibp/config
        sed -i 's@/home/ops/codebase/.*/bin@.@g' *.xml
        sed -i 's@/home/ops/codebase/.*/package@..@g' config.xml
	## Repack it to gzip, then upload it to test environment
	cd /data/bak
	tar zcf $FIBP_Package.$compile_version.$today.tar.gz $FIBP_Package
    mkdir -p $FIBP_Package-build/$compile_version.version
	mv $FIBP_Package.$compile_version.$today.tar.gz $FIBP_Package-build/$compile_version.version/.
	echo "$compile_version version uploads successfully"

  ## If there is error, collect error info from compile.log and send it mail group "fibp"
  else
	echo -e "\nCompile Version: # $compile_version # failed, and email is sent!"
	cat /tmp/compile.$FIBP_DIR_NAME.log | grep -i -A19 -B9 "^make.*error.*" | mail -s "$FIBP_DIR_NAME Build Failed! $today" it@b5m.com
	cat /tmp/compile.$FIBP_DIR_NAME.log > /tmp/invalid.version.$compile_version.$today
	rm -f /tmp/invalid.version.$compile_version.$today
	exit 0
  fi

done

