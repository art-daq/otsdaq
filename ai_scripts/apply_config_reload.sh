#!/bin/bash
# Make a running system use a changed configuration, without restarting ots:
# Halt, Shutdown, Startup, Initialize (and Configure <alias> if given). This is how a new
# active Context or Backbone group (new alias -> group key, new ports, new apps) reaches the
# running Gateway and the other contexts (see gateway-udp-fsm.md, "Applying Active Context
# changes"). A changed Configuration group behind an alias needs only Halt and Configure;
# use this script when in doubt: it works for every kind of change.
#
# Prerequisites: source setup_ots.sh; ots running with Gateway UDP state changes enabled
# (enable_gateway_udp_transitions.sh); ots_drive_udp_test built (otsdaq-utilities).
#
# Usage: apply_config_reload.sh [-a <ip:port>] [-n] [-f] <fsm> [<alias>]
#   fsm     state machine name (ots_drive_udp_test <ip> <port> fsms), e.g. PhysicsRuns0
#   alias   if given, Configure with it at the end (cfg_udp_list_aliases.sh lists them)
#   -a      Gateway UDP address (default: $USER_DATA/ServiceData/GatewayUDPRemoteControl.dat,
#           re-read before every step because Startup may move the port)
#   -n      dry run: print the current state and the steps, send nothing
#   -f      also when a run is going (Running or Paused): Halt ends the run
#
# Stops at the first step that fails, and prints the Gateway log lines of the steps. With an
# alias, the proof is the log line "Attempting Configure transition with Configuration table
# group name: <group>, key: <key>". Exit status: 0 done, 1 a step failed or was refused.

ADDR=""; DRY=0; FORCE=0
while getopts "a:nfh" opt; do
	case $opt in
		a) ADDR=$OPTARG ;;
		n) DRY=1 ;;
		f) FORCE=1 ;;
		*) sed -n 2,22p "$0"; exit 1 ;;
	esac
done
shift $((OPTIND - 1))
FSM=$1; ALIAS=$2
[ -n "$FSM" ] || { sed -n 2,22p "$0"; exit 1; }
command -v ots_drive_udp_test >/dev/null || { echo "ots_drive_udp_test not in PATH: source setup_ots.sh (and build otsdaq-utilities)."; exit 1; }
SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)

gateway_addr() { # prints "ip port"
	if [ -n "$ADDR" ]; then echo "${ADDR%:*} ${ADDR##*:}"; return; fi
	local f="$USER_DATA/ServiceData/GatewayUDPRemoteControl.dat"
	[ -f "$f" ] || { echo "No $f: is ots running with Gateway UDP enabled? (or give -a <ip:port>)" >&2; return 1; }
	awk '{print $1, $2; exit}' "$f"
}

gateway_state() { # the Gateway's status text (same parsing as gateway_udp_cmd.sh)
	local a; a=$(gateway_addr) || return 1
	python3 -I - $a <<'EOF'
import socket, sys, re, html
ip, port = sys.argv[1], int(sys.argv[2])
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM); s.settimeout(5)
s.sendto(b"GetRemoteAppStatus", (ip, port)); out = b""
try:
    while True:
        out += s.recvfrom(65535)[0]
        if b"</ROOT>" in out[-10:]: break
        s.settimeout(1.0)
except socket.timeout:
    pass
m = re.search(r"<name value='[^']*'/>(?:(?!<name value=).)*?<status value='([^']*)'/>"
              r"(?:(?!<name value=).)*?<class value='ots::GatewaySupervisor'/>", out.decode(errors="replace"), re.S)
print(html.unescape(m.group(1)) if m else "")
EOF
}

LOG=$(ls -t "$USER_DATA"/Logs/otsdaq_quiet_run-gateway-*.txt 2>/dev/null | head -1)
LOG_START=$( [ -f "$LOG" ] && wc -l <"$LOG" || echo 0)

STATE=$(gateway_state)
[ -n "$STATE" ] || { echo "No Gateway status over UDP ($(gateway_addr 2>&1)). Check EnableStateChangesOverUDP and EnableApplicationStatusMonitoring."; exit 1; }
echo "Gateway at $(gateway_addr | tr ' ' :), state: ${STATE:0:150}"

STEPS=()
case "$STATE" in
	Running* | Paused*)
		if [ $FORCE = 0 ]; then echo "A run is going ($STATE). Stop it first, or use -f (Halt ends the run)."; exit 1; fi
		STEPS+=(Halt) ;;
	Configured* | Failed*) STEPS+=(Halt) ;;
	Halted*) ;;
	Initial*) STEPS+=(Initialize) ;;
	Shutdown*) STEPS=() ;;
	*) echo "Unexpected state '$STATE': not touching it."; exit 1 ;;
esac
[[ "$STATE" == Shutdown* ]] || STEPS+=(Shutdown)
STEPS+=(Startup Initialize)
[ -n "$ALIAS" ] && STEPS+=("Configure $ALIAS")
printf 'Steps:'; printf ' | %s' "${STEPS[@]}"; echo
[ $DRY = 1 ] && { echo "(dry run: nothing sent)"; exit 0; }

for step in "${STEPS[@]}"; do
	a=$(gateway_addr) || exit 1
	echo
	echo "=== $FSM $step  (Gateway $(echo $a | tr ' ' :))"
	# Startup and Shutdown kill and relaunch contexts: allow time for slow hosts
	OUT=$(ots_drive_udp_test $a "$FSM" $step --timeout 300 2>&1)
	rc=$?
	echo "$OUT" | grep -v "^\s*$" | tail -4
	if [ "$rc" != 0 ]; then
		echo "FAILED: $FSM $step (exit $rc). Gateway state now: $(gateway_state | cut -c1-200)"
		[ -f "$LOG" ] && "$SCRIPT_DIR/gateway_log_events.sh" -n 8 -f "$LOG" | tail -n +2
		exit 1
	fi
done

echo
echo "=== Done. Gateway state: $(gateway_state | cut -c1-150)"
if [ -f "$LOG" ]; then
	echo "Gateway log, lines written by these steps:"
	tail -n +"$((LOG_START + 1))" "$LOG" | LC_ALL=C grep -a -E "Attempting Configure transition|Making System Logbook Entry: Configured with|Error - " |
		sed -e 's/\x1b\[[0-9;]*[A-Za-z]//g' -E -e 's# [^ ]*otsdaq[^ ]*/[A-Za-z0-9_]+\.(cc|h):[0-9]+ \|# |#' | cut -c1-300
fi
exit 0
