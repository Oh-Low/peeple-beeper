#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
make emu
exec ./build/peeple-emu
