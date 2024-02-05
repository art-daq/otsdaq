#!/bin/bash

# Opens a remote file somewhat transparently using less

if [ "x$1" == "x" ]; then
    echo
    echo
    echo "    Usage: vless_ots.sh <node name> <file name>"
    echo
    echo "    e.g.: vless_ots.sh server01.fnal.gov /home/user/ots/Data_user/Logs/otsdaq_quiet_run-gateway-server01.fnal.gov-3055.txt"
    echo "    e.g.: vless_ots.sh server02.fnal.gov /home/user/ots/Data_user/Logs/otsdaq_quiet_run-server02.fnal.gov-3061.txt"
    echo
    echo
    exit
fi 

echo "Opening file in 'less' from node ${1}: $2"

scp ${1}:$2 .tmpLogFile && less .tmpLogFile && rm .tmpLogFile



