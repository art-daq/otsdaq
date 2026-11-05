#!/bin/bash
# Read all data-config registers from the ots_udp_hw_emulator
# via MacroMaker UDP universal read.
#
# Prerequisites:
#   - source setup_ots.sh
#   - ots running and Configured
#   - OTS_MACROMAKER_UDP_PORT and OTS_MACROMAKER_UDP_IP exported
#
# Usage: ./read_emulator_registers.sh [FE_UID]
#   FE_UID defaults to ExampleInterface0

FE_UID="${1:-ExampleInterface0}"
MM_IP="${OTS_MACROMAKER_UDP_IP:-127.0.0.1}"
MM_PORT="${OTS_MACROMAKER_UDP_PORT:-2011}"

echo "Reading emulator registers on $FE_UID via $MM_IP:$MM_PORT"
echo "---"

for addr in 0x1004 0x1005 0x1006 0x1007; do
    result=$(ots_mm_udp_test "$MM_IP" "$MM_PORT" "$FE_UID" r "$addr" 2>&1)
    # extract the TLVL_INFO line with the result
    value=$(echo "$result" | grep "Read " | sed 's/.*= //')
    echo "  $addr = $value"
done
