#!/bin/bash
# Quick health check for a running otsdaq lite instance.
#
# Prerequisites: source setup_ots.sh
#
# Usage: ./check_ots_status.sh

echo "=== otsdaq Lite Status Check ==="
echo

# Check processes
echo "Processes:"
pgrep -fa "xdaq.exe|ots_lite_main|ots_udp_hw_emulator|otsConsoleFwd" | grep -v grep || echo "  (none running)"
echo

# Check UDP ports
echo "UDP Ports:"
ss -ulnp 2>/dev/null | grep -E "2011|2010|4000|30000" || echo "  (no relevant ports bound)"
echo

# Check logs exist
LOG_DIR="${USER_DATA:-Data}/Logs"
echo "Recent logs in $LOG_DIR:"
ls -lt "$LOG_DIR"/*.txt 2>/dev/null | head -5 || echo "  (no logs found)"
echo

# Check TRACE
if command -v trace_cntl &>/dev/null; then
    echo "TRACE buffer: $TRACE_FILE"
    trace_cntl info 2>/dev/null | head -3
else
    echo "TRACE: trace_cntl not in PATH"
fi
echo

# Check MacroMaker UDP
if [ -n "$OTS_MACROMAKER_UDP_PORT" ]; then
    echo "MacroMaker UDP: $OTS_MACROMAKER_UDP_IP:$OTS_MACROMAKER_UDP_PORT"
    timeout 3 ots_mm_udp_test "$OTS_MACROMAKER_UDP_IP" "$OTS_MACROMAKER_UDP_PORT" 2>&1 | grep -E "TLVL_INFO|TLVL_ERROR" | head -5
else
    echo "MacroMaker UDP: not configured (OTS_MACROMAKER_UDP_PORT not set)"
fi
echo

# Check ConfigGUI UDP
if [ -n "$OTS_CONFIG_UDP_PORT" ]; then
    echo "ConfigGUI UDP: $OTS_CONFIG_UDP_IP:$OTS_CONFIG_UDP_PORT"
    ss -ulnp 2>/dev/null | grep ":${OTS_CONFIG_UDP_PORT} " && echo "  (port bound)" || echo "  (port NOT bound)"
else
    echo "ConfigGUI UDP: not configured (OTS_CONFIG_UDP_PORT not set)"
fi
