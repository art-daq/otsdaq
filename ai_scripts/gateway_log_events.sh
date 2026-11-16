#!/bin/bash
# Print only the state-machine events of the Gateway log, so an agent does not grep the
# same patterns by hand: remote (UDP) commands, transitions, the Configure group and key,
# rejected commands ("Error - ..."), UDP State Changer failures, and crashes.
#
# Prerequisites: source setup_ots.sh (USER_DATA). Works on lite, maxlite and full logs.
#
# Usage: gateway_log_events.sh [-n N] [-a] [-s] [-f <log file>]
#   -n N   print the last N events (default 40)
#   -a     also print every ERROR and WARNING line of the Gateway log
#   -s     only events written after the last start of the Gateway (default: whole log)
#   -f     log file (default: the newest $USER_DATA/Logs/otsdaq_quiet_run-gateway-*.txt)
#
# Output: one line per event, ANSI colours removed, long lines cut to 300 characters.
# The time is the one the log line starts with (maxlite: HH:MM:SS.mmm; lite/full: the
# date and time of the TRACE/messagefacility prefix).

N=40; ALL=0; SINCE_START=0; LOG=""
while getopts "n:asf:h" opt; do
	case $opt in
		n) N=$OPTARG ;;
		a) ALL=1 ;;
		s) SINCE_START=1 ;;
		f) LOG=$OPTARG ;;
		*) sed -n 2,16p "$0"; exit 1 ;;
	esac
done

if [ -z "$LOG" ]; then
	[ -n "$USER_DATA" ] || { echo "USER_DATA is not set: source setup_ots.sh, or give -f <log file>."; exit 1; }
	LOG=$(ls -t "$USER_DATA"/Logs/otsdaq_quiet_run-gateway-*.txt 2>/dev/null | head -1)
fi
[ -f "$LOG" ] || { echo "No Gateway log found (${LOG:-$USER_DATA/Logs/otsdaq_quiet_run-gateway-*.txt})."; exit 1; }
echo "Gateway log: $LOG"

# Events (fixed strings from GatewaySupervisor.cc, FiniteStateMachine.cc, ots_lite_main.cc and the C++ runtime)
EVENTS='Received a remote FSM Command|UDP State Changer failed|Attempting Configure transition|Error - |FSM-GatewaySupervisor[[:space:]]*<> transition = |Making System Logbook Entry: Configured with|transition .* attempt failed|Failure performing transition|terminate called|what\(\):|ots_lite_main: fatal|Segmentation fault'
[ $ALL = 1 ] && EVENTS="$EVENTS"'| ERROR | WARNING |:ERROR:|:WARNING:|%MSG-e|%MSG-w'

START_LINE=1
if [ $SINCE_START = 1 ]; then
	# each launch of the Gateway logs its listening line (lite/maxlite) or the XDAQ start banner (full)
	START_LINE=$(LC_ALL=C grep -n -E 'ots_lite_main: listening on|ots_lite_main: context http|Attempting restart on extracted host|XDAQ executive' "$LOG" | tail -1 | cut -d: -f1)
	START_LINE=${START_LINE:-1}
	echo "Events after line $START_LINE (last start of the Gateway)"
fi

tail -n +"$START_LINE" "$LOG" | LC_ALL=C grep -a -E "$EVENTS" | tail -n "$N" |
	sed -e 's/\x1b\[[0-9;]*[A-Za-z]//g' -e 's/[[:space:]]\+/ /g' |
	# drop the source path (file:line) of maxlite/lite lines, keep the time, level and message
	sed -E 's# [^ ]*otsdaq[^ ]*/[A-Za-z0-9_]+\.(cc|h):[0-9]+ \|# |#' |
	cut -c1-300
exit 0
