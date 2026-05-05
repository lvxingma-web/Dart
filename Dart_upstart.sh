#!/bin/bash
LOG_FILE="/home/ganjie/Vision_Code/Dart_upstart.log"
echo "$(date): Dart_upstart.sh start" >> "$LOG_FILE"

export DISPLAY=:0
cd /home/ganjie/Vision_Code/build || {
    echo "$(date): Failed to cd to build dir" >> "$LOG_FILE"
    exit 1
}

if [ ! -x ./Dart ]; then
    echo "$(date): Dart not found or not executable" >> "$LOG_FILE"
    exit 1
fi

exec ./Dart >> "$LOG_FILE" 2>&1