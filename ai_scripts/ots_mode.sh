#!/bin/bash
# Which mode this installation runs (normal, Wiz, Macro Maker, none), the Wiz mode URL with its
# access code, and safe start/stop of this installation only (USER_DATA), for agents.
#
# Prerequisites: source setup_ots.sh
#
# Usage: ots_mode.sh status            mode, processes, and the URL to open (Wiz: with the code)
#        ots_mode.sh wiz [-f]          start Wiz mode (ots -w) and print its URL; refuses when
#                                      normal mode runs (ots -w stops it), unless -f
#        ots_mode.sh normal [-f]       start normal mode (ots); refuses when Wiz mode runs, unless -f
#        ots_mode.sh stop              ots -k: stops only the processes of this USER_DATA
#                                      (never ots -K, which stops every user's ots on the host)
#
# Starting writes the ots output to $USER_DATA/Logs/ots_mode_launch.txt and waits (up to 120 s)
# for the context to answer http. The Wiz URL is
#   http://<host>:<OTS_WIZ_MODE_MAIN_PORT or OTS_MAIN_PORT>/urn:xdaq-application:lid=290/Verify?code=<code>
# with the code from $USER_DATA/ServiceData/OtsWizardData/sequence.out (new at every Wiz start).

ACTION=$1; FORCE=0; [ "$2" = "-f" ] && FORCE=1
[ -n "$USER_DATA" ] || { echo "USER_DATA is not set: source setup_ots.sh first."; exit 1; }
HOST=${THIS_HOST:-$(hostname -f)}
WIZ_PORT=${OTS_WIZ_MODE_MAIN_PORT:-${OTS_MAIN_PORT:-2015}}
LAUNCH_LOG="$USER_DATA/Logs/ots_mode_launch.txt"

procs() { ps -u "$(id -un)" -o pid=,args= | grep "[x]daq.exe" | grep -- "$USER_DATA/"; }
mode() {
	local p; p=$(procs)
	if [ -z "$p" ]; then echo none
	elif echo "$p" | grep -q "otsConfiguration_Wizard_CMake_Run"; then echo wiz
	elif echo "$p" | grep -q "otsConfiguration_MacroMaker_CMake_Run"; then echo macromaker
	else echo normal; fi
}
wiz_url() { echo "http://$HOST:$WIZ_PORT/urn:xdaq-application:lid=290/Verify?code=$(cat "$USER_DATA/ServiceData/OtsWizardData/sequence.out" 2>/dev/null)"; }

status() {
	local m; m=$(mode)
	echo "Mode of $USER_DATA: $m"
	procs | sed -E 's/ -e [^ ]+//; s/^/  /' | cut -c1-160
	case $m in
		wiz) echo "Wiz mode URL: $(wiz_url)" ;;
		normal)
			echo "Gateway: http://$HOST:${OTS_MAIN_PORT:-2015}/ (log in; the ots output prints the full URL)"
			[ -f "$USER_DATA/ServiceData/GatewayUDPRemoteControl.dat" ] &&
				echo "Gateway UDP remote control: $(awk '{print $1":"$2}' "$USER_DATA/ServiceData/GatewayUDPRemoteControl.dat")" ;;
	esac
	return 0
}

wait_http() { # <port>
	local i
	for i in $(seq 1 60); do
		curl -s -m 2 -o /dev/null "http://$HOST:$1" && return 0
		sleep 2
	done
	return 1
}

start() { # <ots option or empty> <wanted mode>
	local cur; cur=$(mode)
	if [ "$cur" = "$2" ]; then echo "Already in $2 mode."; status; return 0; fi
	if [ "$cur" != none ] && [ $FORCE = 0 ]; then
		echo "This installation runs in $cur mode: starting $2 mode stops it. Use -f to do that."; return 1
	fi
	command -v ots >/dev/null || { echo "ots not in PATH: source setup_ots.sh"; return 1; }
	echo "Starting: ots $1  (output: $LAUNCH_LOG)"
	ots $1 >"$LAUNCH_LOG" 2>&1 </dev/null
	local port=${OTS_MAIN_PORT:-2015}; [ "$2" = wiz ] && port=$WIZ_PORT
	if ! wait_http "$port"; then
		echo "No http answer on port $port after 120 s. Last lines of $LAUNCH_LOG:"; tail -15 "$LAUNCH_LOG"; return 1
	fi
	status
}

case "$ACTION" in
	status) status ;;
	wiz) start -w wiz ;;
	normal) start "" normal ;;
	stop)
		[ "$(mode)" = none ] && { echo "Nothing of $USER_DATA is running."; exit 0; }
		echo Y | ots -k >/dev/null 2>&1
		sleep 2
		status ;;
	*) sed -n 2,19p "$0"; exit 1 ;;
esac
