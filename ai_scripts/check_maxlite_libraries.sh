#!/bin/bash
# List the shared libraries an ots maxlite install needs at run time, and flag any that
# maxlite should not need (TRACE, messagefacility, cetlib, fhiclcpp, Boost, artdaq,
# ZeroMQ, CURL, Python, ICU, TBB, SQLite, ROOT).
# Works on an install built for another architecture too (uses readelf, not ldd), so it can
# check a cross-compiled ARM install on the build host.
#
# Prerequisites: readelf (binutils).
#
# Usage:
#   ./check_maxlite_libraries.sh <install dir>     e.g. ~/ots-maxlite/local/install
#
# Prints: the architecture of the programs, every external library needed (with how many
# programs/libraries need it), and the unexpected ones. Exit status 1 if any are unexpected.

INSTALL="${1:-}"
if [ -z "$INSTALL" ] || [ ! -d "$INSTALL" ]; then
    sed -n 2,14p "$0" | sed 's/^# \{0,1\}//'
    exit 1
fi
command -v readelf >/dev/null 2>&1 || { echo "ERROR: readelf not found (package binutils)."; exit 1; }

# every ELF program and shared library in the install
files=()
while IFS= read -r f; do
    head -c 4 "$f" 2>/dev/null | grep -q "ELF" && files+=("$f")
done < <(find "$INSTALL"/bin "$INSTALL"/lib "$INSTALL"/lib64 -maxdepth 1 -type f 2>/dev/null)
[ ${#files[@]} -gt 0 ] || { echo "ERROR: no programs or libraries found under $INSTALL/{bin,lib,lib64}."; exit 1; }

# libraries the install provides itself
declare -A own
for f in "${files[@]}"; do own[$(basename "$f")]=1; done
for f in "$INSTALL"/lib/*.so* "$INSTALL"/lib64/*.so*; do [ -e "$f" ] && own[$(basename "$f")]=1; done

echo "Install: $INSTALL (${#files[@]} programs and libraries)"
echo "Machine: $(readelf -h "${files[0]}" 2>/dev/null | sed -n 's/^ *Machine: *//p' | sort -u | head -1)"
echo
echo "External libraries needed (count of programs/libraries needing each):"
needed=$(for f in "${files[@]}"; do readelf -d "$f" 2>/dev/null | sed -n 's/.*(NEEDED).*\[\(.*\)\]/\1/p'; done | sort | uniq -c)
external=$(echo "$needed" | while read -r count lib; do [ -n "$lib" ] && [ -z "${own[$lib]}" ] && printf "%6d %s\n" "$count" "$lib"; done)
echo "$external"

# checked over everything needed, including libraries the install builds itself (e.g. a lite
# install provides cetlib); otsdaq's own libMessageFacility.so is not messagefacility
unexpected=$(echo "$needed" | awk '{print $2}' | grep -E "^lib(TRACE|trace|MF_|messagefacility|cetlib|fhiclcpp|hep_concurrency|boost_|artdaq|zmq|curl|python|icu|tbb|sqlite|Core\.|RIO\.|Hist\.)" || true)
echo
if [ -n "$unexpected" ]; then
    echo "UNEXPECTED for maxlite:"
    echo "$unexpected" | sed 's/^/  /'
    exit 1
fi
echo "OK: nothing unexpected (expected: C/C++ runtime, Xerces-C, OpenSSL libcrypto)."
