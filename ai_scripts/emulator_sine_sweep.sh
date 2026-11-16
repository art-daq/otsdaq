#!/bin/bash
# Stream sine waves from the hardware emulator and sweep their amplitude slowly. No agent, no tokens: rerun it as often as you like.
#
# This is the "two channels of sine wave, amplitude varying slowly" request from the tutorial "Using AI/SI LLM
# Agents to Control Your Hardware", saved as a script. It only writes emulator registers through MacroMaker UDP.
#
# Prerequisites: source setup_ots.sh; ots running with OTS_MACROMAKER_UDP_IP and OTS_MACROMAKER_UDP_PORT set
#   (see ai_skills/macromaker-udp.md); the State Machine Configured (Running if you want the Visualizer to show it).
#
# Usage: ./emulator_sine_sweep.sh [feUID=ExampleInterface0] [channels=2] [seconds=30]
#   channels  1..8. The emulator's multi-channel mode (register 0x1004 = 2) fixes each channel's shape and rate:
#             channel 0 is a sine of period 100 samples, channel 1 a sine of period 50 (twice the rate), then
#             triangle, square and sawtooth shapes. So with 2 channels you get two sines at different rates.
#   seconds   how long to sweep the amplitude (register 0x1009, % of full scale), one step per second, up and down
#             between 20 and 100 percent. The amplitude register is shared by all channels.
# In the Visualizer (waveform mode) set the channel selector to the same number of channels and "16-bit".
# Register map: ai_skills/emulator-registers.md. Afterwards: write 0 to 0x100000009 to stop the burst.

FE="${1:-ExampleInterface0}"; CH="${2:-2}"; SECS="${3:-30}"
IP="${OTS_MACROMAKER_UDP_IP:-127.0.0.1}"; PORT="${OTS_MACROMAKER_UDP_PORT}"
[ -z "$PORT" ] && { sed -n 2,19p "$0"; echo; echo "ERROR: OTS_MACROMAKER_UDP_PORT is not set"; exit 1; }
command -v ots_mm_udp_test >/dev/null || { echo "ERROR: ots_mm_udp_test not found (source setup_ots.sh)"; exit 1; }

# ots_mm_udp_test reads register values as HEX: pass decimal numbers through printf so 50 means 50
w() { ots_mm_udp_test "$IP" "$PORT" "$FE" w "$1" "$2" 2>&1 | grep -E "Wrote|rror" | sed 's/.*|[[:space:]]*//'; }
wd() { w "$1" "$(printf '0x%X' "$2")"; }   # write a DECIMAL value

echo "emulator $FE at $IP:$PORT: $CH channels, 16-bit sine waves, amplitude sweep for ${SECS}s"
w 0x1004 2          # data mode 2: multi-channel waveforms
w 0x1005 0x10       # 16-bit samples
wd 0x1007 "$CH"     # number of channels
wd 0x1006 0         # samples per packet: 0 = as many as fit (1 would give the Visualizer a single point per packet)
wd 0x100A 50        # offset 50 percent of full scale
w 0x100000009 1     # burst data enable
w 0x1001 0xFFFF     # data count (hex): a long burst
amp=20; step=10
for ((t = 0; t < SECS; t++)); do
    wd 0x1009 "$amp"                      # amplitude, percent of full scale
    amp=$((amp + step))
    if [ $amp -ge 100 ]; then amp=100; step=-10; elif [ $amp -le 20 ]; then amp=20; step=10; fi
    sleep 1
done
echo "done: the burst continues at the last amplitude; stop it with: ots_mm_udp_test $IP $PORT $FE w 0x100000009 0"
