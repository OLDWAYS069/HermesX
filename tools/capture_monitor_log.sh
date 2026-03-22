#!/bin/zsh
set -euo pipefail

env_name="${1:-heltec-wireless-tracker}"
if [[ $# -gt 0 ]]; then
  shift
fi

log_dir="${LOG_CAPTURE_DIR:-logs/monitor}"
timestamp="$(date +%Y%m%d_%H%M%S)"
mkdir -p "$log_dir"

log_file="${log_dir}/${env_name}_monitor_${timestamp}.log"

echo "Environment: ${env_name}"
echo "Log file: ${log_file}"
echo "Press Ctrl-C to stop capture."

platformio device monitor -e "$env_name" "$@" | tee "$log_file"
