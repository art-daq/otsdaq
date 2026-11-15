#!/bin/bash
# Start, stop or show the hardware emulator (ots_udp_hw_emulator) of ONE port, without
# touching emulators of other installations on the same host (never killall).
#
# Prerequisites: source setup_ots.sh (ots_udp_hw_emulator in PATH, USER_DATA).
#
# Usage: emulator_ctl.sh start|stop|status|restart <port>
#   start    refuse if the UDP port is already in use; start the emulator in the background,
#            write its pid to $USER_DATA/ServiceData/emulator_<port>.pid and its output to
#            $USER_DATA/Logs/ots_udp_hw_emulator-<port>.txt
#   stop     stop the emulator that listens on <port>, only if it is an ots_udp_hw_emulator
#            process of this user (found by the port, so an emulator started by hand is
#            found too); removes the pid file
#   status   which process listens on <port>, its start time, and the pid file
#
# The port is the emulator's UDP port: the FE interface table points at it (InterfaceIPAddress,
# InterfacePort of the FE, e.g. 4000 in the tutorial data). Exit status: 0 ok, 1 error/refused.

ACTION=$1; PORT=$2
if [ -z "$ACTION" ] || ! [[ "$PORT" =~ ^[0-9]+$ ]]; then sed -n 2,17p "$0"; exit 1; fi
[ -n "$USER_DATA" ] || { echo "USER_DATA is not set: source setup_ots.sh first."; exit 1; }
PIDFILE="$USER_DATA/ServiceData/emulator_${PORT}.pid"
LOGFILE="$USER_DATA/Logs/ots_udp_hw_emulator-${PORT}.txt"

# pid of the process holding UDP <port> (empty if none or not visible to this user)
port_pid() { ss -lunpH "sport = :$PORT" 2>/dev/null | grep -oP 'pid=\K[0-9]+' | head -1; }
port_busy() { [ -n "$(ss -lunH "sport = :$PORT" 2>/dev/null)" ]; }
is_our_emulator() { # <pid>: an ots_udp_hw_emulator of this user
	[ -n "$1" ] && [ "$(ps -o user= -p "$1" 2>/dev/null)" = "$(id -un)" ] &&
		ps -o args= -p "$1" 2>/dev/null | grep -q "ots_udp_hw_emulator"
}

status() {
	local pid; pid=$(port_pid)
	if [ -n "$pid" ]; then
		echo "UDP port $PORT: pid $pid ($(ps -o user=,lstart=,args= -p "$pid" 2>/dev/null))"
	elif port_busy; then
		echo "UDP port $PORT: in use by a process of another user"
	else
		echo "UDP port $PORT: free (no emulator)"
	fi
	[ -f "$PIDFILE" ] && echo "pid file $PIDFILE: $(cat "$PIDFILE")"
	return 0
}

stop() {
	local pid; pid=$(port_pid)
	if [ -z "$pid" ]; then
		port_busy && { echo "UDP port $PORT is held by a process of another user: not touching it."; return 1; }
		echo "No emulator on UDP port $PORT."; rm -f "$PIDFILE"; return 0
	fi
	if ! is_our_emulator "$pid"; then
		echo "UDP port $PORT is held by pid $pid ($(ps -o args= -p "$pid")), not an ots_udp_hw_emulator of $(id -un): not touching it."
		return 1
	fi
	echo "Stopping ots_udp_hw_emulator pid $pid (UDP port $PORT)"
	kill "$pid"
	for i in 1 2 3 4 5; do kill -0 "$pid" 2>/dev/null || break; sleep 1; done
	kill -0 "$pid" 2>/dev/null && { echo "Still running after 5 s: kill -9 $pid"; kill -9 "$pid"; }
	rm -f "$PIDFILE"
	echo "Stopped."
}

start() {
	command -v ots_udp_hw_emulator >/dev/null || { echo "ots_udp_hw_emulator not in PATH: source setup_ots.sh"; return 1; }
	if port_busy; then
		echo "UDP port $PORT is already in use:"; status; return 1
	fi
	mkdir -p "$(dirname "$PIDFILE")" "$(dirname "$LOGFILE")"
	nohup ots_udp_hw_emulator "$PORT" >"$LOGFILE" 2>&1 </dev/null &
	local pid=$!
	echo "$pid" >"$PIDFILE"
	sleep 1
	if ! kill -0 "$pid" 2>/dev/null; then
		echo "The emulator exited at once. Its output ($LOGFILE):"; tail -5 "$LOGFILE"; rm -f "$PIDFILE"; return 1
	fi
	echo "Started ots_udp_hw_emulator pid $pid on UDP port $PORT (log $LOGFILE)"
}

case "$ACTION" in
	start) start ;;
	stop) stop ;;
	restart) stop && start ;;
	status) status ;;
	*) sed -n 2,17p "$0"; exit 1 ;;
esac
