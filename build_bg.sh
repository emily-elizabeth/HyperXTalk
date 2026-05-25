#!/bin/bash
set -e
cd "$(dirname "$0")"
LOG="build-linux-x86_64.log"

echo "Starting background build at $(date)" > "$LOG"
echo "Tailing log: tail -f $(pwd)/$LOG" >> "$LOG"

nohup make compile-linux-x86_64 -j4 >> "$LOG" 2>&1 &
PID=$!
echo "Build PID: $PID"
echo "To monitor: tail -f $(pwd)/$LOG"
echo "To stop:    kill $PID"
