#!/bin/sh
# Build fast5.nro (the launcher). Build the wrapper first (../build.sh):
# the NRO carries ../fast5.nsp and ../fast5.build.
HERE="$(cd "$(dirname "$0")" && pwd)"
LAUNCHER_DIR="$HERE" PAYLOAD=fast5 exec "$HERE/../runtime/launcher/build.sh" "$@"
