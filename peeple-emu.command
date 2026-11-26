#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
make bench
exec ./build/peeple-emu
