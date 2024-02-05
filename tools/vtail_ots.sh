#!/bin/bash

# Opens a remote file somewhat transparently using tail -f

if [ "x$1" == "x" ]; then
    echo
    echo
    echo "    Usage: vtail_ots.sh <node name> <file name>"
    echo
    echo "    e.g.: vtail_ots.sh server01.fnal.gov /home/user/ots/Data_user/Logs/otsdaq_quiet_run-gateway-server01.fnal.gov-3055.txt"
    echo
    echo
    exit
fi 

echo "Viewing file with 'tail -f' from node ${1}: $2"

ssh ${1} tail -f $2


