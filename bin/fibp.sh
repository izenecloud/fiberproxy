#!/bin/bash
NAME=FIBP
PNAME=FibpServerProcess

CONSOLE_LOG_DIR="./consolelog"
mkdir -p $CONSOLE_LOG_DIR
TIMENOW=`date +%m-%d.%H%M`
LOG_FILE=$CONSOLE_LOG_DIR/log.$TIMENOW.log
 case "$1" in
  start)
    PROCESS_NUM=`ps -ef|grep "FibpServerProcess"|grep -v grep|wc -l`
    if [ $PROCESS_NUM -eq 1 ]
      then
        echo -e "$NAME has already started!\n"
        exit 0
      else
        echo -e "Starting $NAME..."
        ./$PNAME -F config >> $LOG_FILE 2>&1 &
        sleep 2
        PROCESS_NUM=`ps -ef|grep "$PNAME"|grep -v grep|wc -l`
        if [ $PROCESS_NUM -eq 1 ]
          then
            echo -e "Success. \nTo monitor the log file, type: 'tail -f $LOG_FILE'\n"
          else
            echo -e "Fail.\nCheck log $LOG_FILE for more detail.\n"
        fi
    fi
    ;;
  stop)
    echo -e "Stopping $NAME..."
    ps -ef|grep "$PNAME"|grep -v grep|awk '{print $2}'|xargs kill -2 2>/dev/null
    sleep 1
    echo -e "Done.\n"
    ;;
  restart)
    echo -e "Restarting $NAME..."
    ps -ef|grep "$PNAME"|grep -v grep|awk '{print $2}'|xargs kill -2 2>/dev/null
    sleep 1
    ./$PNAME -F config >> $LOG_FILE 2>&1 &
    sleep 4
    PROCESS_NUM=`ps -ef|grep "$PNAME"|grep -v grep|wc -l`
    if [ $PROCESS_NUM -eq 1 ]
      then
        echo -e "Success. \nTo monitor the log file, type: 'tail -f $LOG_FILE'\n"
      else
        echo -e "Fail.\nCheck log $LOG_FILE for more detail.\n"
    fi
    ;;
  *)
    echo -e "Usage: ./fibp.sh  { start | stop | restart }\n" >&2
    exit 1
    ;;
 esac
exit 0
