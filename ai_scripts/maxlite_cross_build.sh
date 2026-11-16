#!/bin/bash
# Cross-compile ots maxlite for 64-bit ARM (aarch64) or 32-bit ARM (arm32) on an x86 Linux host,
# inside a Debian 12 container (podman) that has the cross compilers and the target Xerces-C and
# OpenSSL packages. Nothing is installed on the host. Recipe: ../ai_skills/ots-maxlite-architecture.md
#
# Prerequisites: podman (rootless is fine), network access (Debian packages, cetmodules, cmake,
# tutorial data), the otsdaq-demo repo (for the Containerfile and the installer).
#
# Usage:
#   ./maxlite_cross_build.sh <aarch64|arm32> <new install dir> [--repo-base <dir with the repos>] [--branch <b>]
#
#   default --branch feature/ots-maxlite; --repo-base a local directory holding otsdaq,
#   otsdaq-utilities, otsdaq-components, otsdaq-demo (mounted read-only into the container),
#   or a URL (default https://github.com/art-daq).
#
# Result: <install dir>/local/install with ARM programs and libraries (check them with
# check_maxlite_libraries.sh), and a setup_ots.sh whose mb/mz cross-compile again (run them in
# the same container: see the printed command).
#
# Host notes (found on AlmaLinux 9):
#   - no /etc/subuid entry for the user (podman can't map container users): images are unpacked
#     with vfs + ignore_chown_errors in a separate storage, PODMAN_ROOT (default
#     ~/.local/share/ots-maxlite-cross-podman; run root PODMAN_RUNROOT), so the user's normal
#     podman storage is untouched
#   - apt in such a container cannot setgroups: the Containerfile keeps apt as root
#   - SELinux enforcing: the container runs with --security-opt label=disable instead of
#     relabeling host directories

ARCH="$1"; DEST="$2"; shift 2 2>/dev/null
REPO_BASE=https://github.com/art-daq; BRANCH=feature/ots-maxlite
while [ $# -gt 0 ]; do
    case "$1" in
        --repo-base) REPO_BASE="$2"; shift;; --branch) BRANCH="$2"; shift;;
        *) ARCH="";; esac; shift
done
case "$ARCH" in aarch64|arm32) ;; *) sed -n 2,26p "$0" | sed 's/^# \{0,1\}//'; exit 1;; esac
[ -n "$DEST" ] || { sed -n 2,26p "$0" | sed 's/^# \{0,1\}//'; exit 1; }
command -v podman >/dev/null || { echo "ERROR: podman not found."; exit 1; }
if [ -e "$DEST" ] && [ -n "$(ls -A "$DEST" 2>/dev/null)" ]; then echo "ERROR: $DEST exists and is not empty."; exit 1; fi

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# the Containerfile and installer come from otsdaq-demo: next to this otsdaq checkout, or in --repo-base
DEMO=""
for d in "$SCRIPT_DIR/../../otsdaq-demo" "$REPO_BASE/otsdaq-demo"; do
    [ -f "$d/tools/ots-maxlite-cross.Containerfile" ] && { DEMO=$(cd "$d" && pwd); break; }
done
[ -n "$DEMO" ] || { echo "ERROR: otsdaq-demo/tools/ots-maxlite-cross.Containerfile not found (next to otsdaq or in --repo-base)."; exit 1; }

PODMAN_ROOT=${PODMAN_ROOT:-$HOME/.local/share/ots-maxlite-cross-podman}
PODMAN_RUNROOT=${PODMAN_RUNROOT:-/tmp/ots-maxlite-cross-podman-$USER}  # a storage stays tied to the run root it was created with
P="podman --root $PODMAN_ROOT/storage --runroot $PODMAN_RUNROOT --storage-driver vfs --storage-opt vfs.ignore_chown_errors=true"
IMAGE=localhost/ots-maxlite-cross

if ! $P image exists $IMAGE; then
    echo "Building the cross-build image (first time only, a few minutes)..."
    $P build -t $IMAGE -f "$DEMO/tools/ots-maxlite-cross.Containerfile" "$DEMO/tools" || { echo "ERROR: image build failed."; exit 1; }
fi

mkdir -p "$DEST" && DEST=$(cd "$DEST" && pwd)
cp "$DEMO/tools/ots-quick-maxlite-start.sh" "$DEST/" && chmod 755 "$DEST/ots-quick-maxlite-start.sh"
MOUNTS=(-v "$DEST:$DEST")
if [ -d "$REPO_BASE" ]; then REPO_BASE=$(cd "$REPO_BASE" && pwd); MOUNTS+=(-v "$REPO_BASE:$REPO_BASE:ro"); fi

echo "Cross compiling ots maxlite for $ARCH into $DEST (branch $BRANCH, repos from $REPO_BASE)..."
start=$(date +%s)
$P run --rm --security-opt label=disable "${MOUNTS[@]}" -w "$DEST" $IMAGE \
    ./ots-quick-maxlite-start.sh --branch "$BRANCH" --repo-base "$REPO_BASE" --arch "$ARCH" > "$DEST/cross-install.log" 2>&1
rc=$?
echo "Installer exit $rc after $(( $(date +%s) - start )) s; log: $DEST/cross-install.log"
[ -f "$DEST/tmp/build-timings.txt" ] && cat "$DEST/tmp/build-timings.txt"
[ $rc -eq 0 ] || exit $rc
"$SCRIPT_DIR/check_maxlite_libraries.sh" "$DEST/local/install"
echo
echo "To rebuild after source changes (mb = incremental, mz = clean), in the same container:"
echo "  $P run --rm --security-opt label=disable ${MOUNTS[*]} -w $DEST $IMAGE bash -c 'source setup_ots.sh >/dev/null; mb'"
