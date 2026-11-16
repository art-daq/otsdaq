#!/usr/bin/env python3
"""read_raw_data.py - decode a raw data file written by the RawDataSaverConsumer
(e.g. $USER_DATA/OutputData/Run_Run2_Raw.dat) from the hardware emulator (ots_udp_hw_emulator).

File layout: the emulator's UDP packets, back to back, nothing in between (RawDataSaverConsumer adds
no framing; OtsDataSaverConsumer adds one byte before each packet: use --ots-saver). Each packet is
  byte 0     type: 1 = first of a burst, 2 = middle, 3 = last
  byte 1     sequence number, +1 per packet, wraps at 256
  bytes 2..  payload: data mode 0 (counter): one 8-byte little-endian counter;
             modes 1-3 (waveforms): samples, little-endian, channel-interleaved
             (sample 0 of channel 0, 1, ..., then sample 1 ...), 1, 2 or 4 bytes per sample
The packet length is not in the file: it is found from the headers (every packet of a run has the
same length). The number of channels and the bit width are not in the data either: give them
(emulator registers 0x1007 and 0x1005, read_emulator_registers.sh; emulator-registers.md), default 1 and 16.

Usage: read_raw_data.py <file> [--channels N] [--bits 8|12|16|32] [--show K] [--csv out.csv] [--ots-saver]
  prints the packet length, counts per type, sequence gaps, and per channel the number of samples,
  min, max, mean and the first K samples (default 8); --csv writes every sample (one row per sample
  time, one column per channel). Exit status: 0 ok, 1 cannot decode.
"""
import argparse
import struct
import sys


def find_packet_length(b, prefix):
    """smallest L such that every L-byte step lands on a header with type 1-3 and sequence +1; the last packet
    of a file may be shorter than L (the "last of the burst" packet carries what was left)"""
    step0 = 1 if prefix else 0
    for L in range(3, min(len(b), 70000) + 1):
        stride = L + step0
        ok = True
        prev = None
        for off in range(0, len(b), stride):
            if off + step0 + 1 >= len(b):  # a final fragment too short to hold a header
                ok = False
                break
            t, s = b[off + step0], b[off + step0 + 1]
            if t not in (1, 2, 3) or (prev is not None and s != (prev + 1) % 256):
                ok = False
                break
            prev = s
        if ok and len(b) - ((len(b) - 1) // stride) * stride >= 2 + step0:
            return L
    return None


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("file")
    ap.add_argument("--channels", type=int, default=0, help="default 1 (with a hint: the file does not record it)")
    ap.add_argument("--bits", type=int, default=16, choices=(8, 12, 16, 32))
    ap.add_argument("--show", type=int, default=8)
    ap.add_argument("--csv")
    ap.add_argument("--ots-saver", action="store_true", help="one byte (quad-word count) before each packet")
    a = ap.parse_args()

    b = open(a.file, "rb").read()
    print("%s: %d bytes" % (a.file, len(b)))
    if not b:
        print("Empty file: no packet was saved (was the run started with the emulator streaming?).")
        return 0
    L = find_packet_length(b, a.ots_saver)
    lead = 0
    if L is None:
        # the file may begin with a leftover packet of the previous burst (the "last" packet, any size): look for
        # the uniform stream after it, at least 3 packets long
        for lead in range(1, min(len(b), 4000)):
            if b[lead] in (1, 2, 3):
                L2 = find_packet_length(b[lead:], a.ots_saver)
                if L2 is not None and len(b) - lead >= 3 * (L2 + (1 if a.ots_saver else 0)):
                    L = L2
                    break
        if L is None:
            lead = 0
        else:
            print("Skipped %d leading byte(s): a leftover packet of the previous burst (type %d)." % (lead, b[0]))
            b = b[lead:]
    if L is None:
        print("Cannot find a packet length: not an emulator raw file, or packets of different sizes"
              " (try --ots-saver for files of the OtsDataSaverConsumer).")
        return 1
    stride = L + (1 if a.ots_saver else 0)
    packets = [b[o + stride - L:o + stride] for o in range(0, len(b), stride)]
    short = len(packets[-1]) < L
    types = [p[0] for p in packets]
    seqs = [p[1] for p in packets]
    print("Packets: %d of %d bytes (payload %d); first %d, middle %d, last %d; sequence %d..%d"
          % (len(packets), L, L - 2, types.count(1), types.count(2), types.count(3), seqs[0], seqs[-1]))
    if short:
        print("The last packet is shorter (%d bytes): the end of the burst." % len(packets[-1]))
    gaps = sum(1 for x, y in zip(seqs, seqs[1:]) if y != (x + 1) % 256)
    if gaps:
        print("Sequence gaps: %d (lost packets)" % gaps)

    guess = a.channels == 0
    a.channels = a.channels or 1
    if L == 10 and a.channels == 1 and a.bits == 16 and len(packets) > 0:
        counters = [struct.unpack("<Q", p[2:10])[0] for p, t in zip(packets, types) if t != 3]  # 3 closes the burst
        print("Payload is 8 bytes: data mode 0 (counter) assumed. Counters: %s%s"
              % (", ".join(str(c) for c in counters[:a.show]), " ..." if len(counters) > a.show else ""))
        if len(counters) > 1:
            d = [y - x for x, y in zip(counters, counters[1:])]
            print("Counter steps: min %d, max %d" % (min(d), max(d)))
        return 0

    bps = {8: 1, 12: 2, 16: 2, 32: 4}[a.bits]
    fmt = {1: "<B", 2: "<H", 4: "<I"}[bps]
    group = bps * a.channels
    if (L - 2) % group:
        print("Payload of %d bytes is not a whole number of %d-channel %d-bit samples: check --channels/--bits."
              % (L - 2, a.channels, a.bits))
        return 1
    chans = [[] for _ in range(a.channels)]
    for p in packets:
        pay = p[2:]
        for off in range(0, len(pay) - group + 1, group):  # whole samples only (the last packet may be short)
            for ch in range(a.channels):
                chans[ch].append(struct.unpack_from(fmt, pay, off + ch * bps)[0])
    full = (1 << a.bits) - 1
    if guess:
        print("Hint: the file does not say how many channels it holds (data mode 2 interleaves the number set in"
              " register 0x1007): read the emulator's settings with read_emulator_registers.sh and pass --channels.")
    if a.bits == 16 and max(max(v) for v in chans) <= 4095:
        print("Hint: no value above 4095: the emulator was probably set to 12 bits (--bits 12 for percentages).")
    print("Samples: %d per channel per packet, %d channel(s), %d bits (full scale %d)"
          % ((L - 2) // group, a.channels, a.bits, full))
    for ch, v in enumerate(chans):
        print("  channel %d: %d samples, min %d, max %d, mean %.1f (%.0f%%-%.0f%% of full scale); first: %s"
              % (ch, len(v), min(v), max(v), sum(v) / len(v), 100.0 * min(v) / full, 100.0 * max(v) / full,
                 " ".join(str(x) for x in v[:a.show])))
    if a.csv:
        with open(a.csv, "w") as f:
            f.write(",".join("ch%d" % c for c in range(a.channels)) + "\n")
            for i in range(len(chans[0])):
                f.write(",".join(str(chans[c][i]) for c in range(a.channels)) + "\n")
        print("Wrote %s" % a.csv)
    return 0


if __name__ == "__main__":
    sys.exit(main())
