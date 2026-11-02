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
    # Replies are sent in 1500-byte datagrams (TransceiverSocket::acknowledge
    # default chunk size), so keep reading until the XML root closes or the
    # socket goes quiet. Non-XML replies (Help, Done, error text) are one line.
    python3 -I - "$IP" "$PORT" "$1" "$2" <<'EOF'
import socket, sys
ip, port, msg, t = sys.argv[1], int(sys.argv[2]), sys.argv[3], float(sys.argv[4])
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.settimeout(t)
s.sendto(msg.encode(), (ip, port))
out = b""
try:
    while True:
        out += s.recvfrom(65535)[0]
        if b"</ROOT>" in out[-10:]:
            break
        if not out.lstrip().startswith(b"<"):
            break
        s.settimeout(1.0)  # later chunks follow immediately
except socket.timeout:
    pass
sys.stdout.write(out.decode(errors="replace"))
EOF
}

gateway_status() {
    # The Gateway entry is the one whose <class> is ots::GatewaySupervisor; its
    # LID is whatever the XDAQ config assigns, so do not rely on it being first.
    # Elements are emitted in order name, id, status, ..., class per app.
    # The XML goes in as an argument: a heredoc already occupies python's stdin
    # (the script), so piping the XML in as well silently delivers nothing.
    python3 -I - "$(send GetRemoteAppStatus 5)" <<'EOF'
import re, sys, html
xml = sys.argv[1]
m = re.search(r"<name value='[^']*'/>(?:(?!<name value=).)*?<status value='([^']*)'/>"
              r"(?:(?!<name value=).)*?<class value='ots::GatewaySupervisor'/>", xml, re.S)
print(html.unescape(m.group(1)) if m else "")
EOF
}

# Status before sending. Text from an earlier failed transition persists until
# the next one succeeds, so a "Failed:::" seen right away may be left over.
PRE=""
case "$MSG" in
    Get*|Help*|help*) ;;
    *) if [ -n "$EXPECT" ]; then
           PRE=$(gateway_status)
           if [ -z "$PRE" ]; then
               echo "No Gateway status in the GetRemoteAppStatus reply."
               echo "Check EnableApplicationStatusMonitoring=Yes and the UDP IP/port."
               exit 3
           fi
           echo "Status before: ${PRE:0:120}"
       fi ;;
esac

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
        Failed:::*)
            [ "$ST" = "$PRE" ] && echo "(note: identical to the status before sending; may be left over from an earlier attempt)"
            echo "FAILED: $ST"; exit 1 ;;
        "$EXPECT") echo "Reached $EXPECT"; exit 0 ;;
    esac
    echo "  status: ${ST:0:120}"
done
echo "Timed out after ${TMO}s waiting for $EXPECT"
exit 2
