#!/bin/bash

# Opens a remote file somewhat transparently using less

if [ "x$1" == "x" ]; then
    echo
    echo
    echo "    Usage: vless_ots.sh <ots log file name> [-g \"grep pattern\"]"
    echo
    echo "    e.g.: vless_ots.sh /home/user/ots/Data_user/Logs/otsdaq_quiet_run-gateway-server01.fnal.gov-3055.txt"
    echo "    e.g.: vless_ots.sh /home/user/ots/Data_user/Logs/otsdaq_quiet_run-gateway-server01.fnal.gov-3055.txt -g \"CDR lock\|Phase 2b\""
    echo
    echo
    exit
fi

remote_path="$1"
grep_pattern=""
if [ "x$2" == "x-g" ] && [ "x$3" != "x" ]; then
    grep_pattern="$3"
fi
hostname=""

if [[ "$1" =~ ^([a-zA-Z0-9.-]+):(.+)$ ]]; then
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

if [ "x$hostname" == "x" ]; then
    echo
    echo "    Error: Could not determine hostname from the provided path: $1"
    echo
    echo "    Supported formats:"
    echo "      host:/absolute/path/to/file"
    echo "      host:relative/path/to/file"
    echo "      otsdaq_quiet_run-gateway-<hostname>-<pid>.txt"
    echo "      otsdaq_quiet_run-<hostname>-<pid>.txt"
    echo "      <prefix>launch_attempt_<hostname>_<suffix>"
    echo
    exit 1
fi

echo "Opening file in 'less' from node $hostname: $remote_path"

# Build list of hostnames to try: original + stripped alt (if -data/-ipmi suffix)
_vless_hosts=("$hostname")
if [[ "$hostname" == *"-data."* || "$hostname" == *"-data" ]]; then
    _vless_hosts+=("${hostname/-data/}")
elif [[ "$hostname" == *"-ipmi."* || "$hostname" == *"-ipmi" ]]; then
    _vless_hosts+=("${hostname/-ipmi/}")
fi

# Try all hosts in parallel; use the first one that succeeds
_vless_tmpdir=$(mktemp -d /tmp/vless_ots_XXXXXX)
declare -A _vless_pids
for _vless_h in "${_vless_hosts[@]}"; do
    _vless_safe="${_vless_h//[^a-zA-Z0-9._-]/_}"
    ( scp -o BatchMode=yes -o ConnectTimeout=2 "${_vless_h}:${remote_path}" "${_vless_tmpdir}/${_vless_safe}.tmp" >/dev/null 2>&1
      echo $? > "${_vless_tmpdir}/${_vless_safe}.rc" ) &
    _vless_pids["$_vless_h"]=$!
done

# Poll until one succeeds or all finish
_vless_winner=""
while [[ -z "$_vless_winner" ]]; do
    _vless_all_done=1
    for _vless_h in "${_vless_hosts[@]}"; do
        _vless_safe="${_vless_h//[^a-zA-Z0-9._-]/_}"
        if [[ -f "${_vless_tmpdir}/${_vless_safe}.rc" ]]; then
            if [[ "$(cat "${_vless_tmpdir}/${_vless_safe}.rc")" == "0" ]]; then
                _vless_winner="$_vless_h"
                break
            fi
        else
            _vless_all_done=0  # still running
        fi
    done
    [[ -n "$_vless_winner" || "$_vless_all_done" == "1" ]] && break
    sleep 0.1
done

# Kill any still-running background jobs
for _vless_h in "${_vless_hosts[@]}"; do
    kill "${_vless_pids[$_vless_h]}" 2>/dev/null
    wait "${_vless_pids[$_vless_h]}" 2>/dev/null
done

if [[ -z "$_vless_winner" ]]; then
    echo "  Error: Could not retrieve file from any of: ${_vless_hosts[*]}"
    rm -rf "$_vless_tmpdir"
    unset _vless_hosts _vless_pids _vless_tmpdir _vless_winner _vless_h _vless_safe _vless_all_done
    exit 1
fi

_vless_safe_winner="${_vless_winner//[^a-zA-Z0-9._-]/_}"
[[ "$_vless_winner" != "$hostname" ]] && echo "  Note: connected via ${_vless_winner}"
cp "${_vless_tmpdir}/${_vless_safe_winner}.tmp" .tmpLogFile
rm -rf "$_vless_tmpdir"
unset _vless_hosts _vless_pids _vless_tmpdir _vless_winner _vless_h _vless_safe _vless_all_done _vless_safe_winner

if [ "x$grep_pattern" != "x" ]; then
    grep -E -- "$grep_pattern" .tmpLogFile > .tmpLogFile.grep
    less .tmpLogFile.grep && rm .tmpLogFile .tmpLogFile.grep
else
    less .tmpLogFile && rm .tmpLogFile
fi
