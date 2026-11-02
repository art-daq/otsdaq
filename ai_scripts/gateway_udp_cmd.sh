#!/bin/bash
# Send a Gateway UDP message (FSM command or query) and poll the Gateway status.
#
# Prerequisites: ots running with EnableStateChangesOverUDP, IP/port set, and
# EnableApplicationStatusMonitoring=Yes (see ai_skills/gateway-udp-fsm.md).
#
# Usage: gateway_udp_cmd.sh <ip> <port> <message> [expectedState] [timeoutSeconds]
#   message: e.g. PhysicsRuns0,Configure,defaultSystemAlias | GetStateMachineNames
#   expectedState: e.g. Configured. If given, polls until reached or Failed.
#                  If omitted, just prints the reply (queries) or the status once.

IP="$1"; PORT="$2"; MSG="$3"; EXPECT="$4"; TMO="${5:-120}"
[ -z "$MSG" ] && { sed -n 2,11p "$0"; exit 1; }

send() {
    python3 -I - "$IP" "$PORT" "$1" "$2" <<'EOF'
import socket, sys
ip, port, msg, t = sys.argv[1], int(sys.argv[2]), sys.argv[3], float(sys.argv[4])
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.settimeout(t)
s.sendto(msg.encode(), (ip, port))
try:
    print(s.recvfrom(65535)[0].decode(errors="replace"))
except socket.timeout:
    pass
EOF
}

gateway_status() {
    # first <status> in GetRemoteAppStatus is the Gateway; unescape XML entities
    send GetRemoteAppStatus 5 | grep -m1 "<status" \
        | sed "s/.*value='//;s/'\/>.*//;s/&apos;/'/g;s/&lt;/</g;s/&gt;/>/g;s/&amp;/\&/g"
}

REPLY=$(send "$MSG" 3)
[ -n "$REPLY" ] && echo "$REPLY"

case "$MSG" in
    Get*|Help*|help*) exit 0 ;;
esac

[ -z "$EXPECT" ] && { echo "Status: $(gateway_status)"; exit 0; }

END=$((SECONDS + TMO))
while [ $SECONDS -lt $END ]; do
    sleep 3
    ST=$(gateway_status)
    case "$ST" in
        Failed:::*) echo "FAILED: $ST"; exit 1 ;;
        "$EXPECT") echo "Reached $EXPECT"; exit 0 ;;
    esac
    echo "  status: ${ST:0:120}"
done
echo "Timed out after ${TMO}s waiting for $EXPECT"
exit 2
