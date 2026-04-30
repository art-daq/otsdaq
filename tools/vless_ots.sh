#!/bin/bash

# Opens a remote file somewhat transparently using less

if [ "x$1" == "x" ]; then
    echo
    echo
    echo "    Usage: vless_ots.sh <ots log file name>"
    echo
    echo "    e.g.: vless_ots.sh /home/user/ots/Data_user/Logs/otsdaq_quiet_run-gateway-server01.fnal.gov-3055.txt"
    echo
    echo
    exit
fi

remote_path="$1"
hostname=""

if [[ "$1" =~ ^([a-zA-Z0-9.-]+):(\/.*)$ ]]; then
    hostname="${BASH_REMATCH[1]}"
    remote_path="${BASH_REMATCH[2]}"
else
    basename=$(basename "$1")
    #parse as gateway log file first, then non-gateway
    hostname=$(echo "$basename" | sed -n 's/otsdaq_quiet_run-gateway-\([a-zA-Z0-9.-]*\)-[0-9]*\.txt/\1/p')
    if [ "x$hostname" == "x" ]; then
        hostname=$(echo "$basename" | sed -n 's/otsdaq_quiet_run-\([a-zA-Z0-9.-]*\)-[0-9]*\.txt/\1/p')
    fi
    if [ "x$hostname" == "x" ]; then #then as artdaq file
        hostname=$(echo "$basename" | sed -n 's/.*launch_attempt_\([^_]*\)_.*/\1/p')
    fi
fi
echo "$hostname"

echo "Opening file in 'less' from node $hostname: $remote_path"

scp "${hostname}:${remote_path}" .tmpLogFile && less .tmpLogFile && rm .tmpLogFile
