#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: scripts/select_docs.sh \"task description\""
  exit 1
fi

query="$*"
q="$(printf '%s' "$query" | tr '[:upper:]' '[:lower:]')"

skill_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
repo_root="$(cd "${skill_dir}/../../.." && pwd)"
docs_dir="${repo_root}/docs"

declare -a results=()

add_doc() {
  local file="$1"
  local reason="$2"
  local path="${docs_dir}/${file}"
  if [[ -f "$path" ]]; then
    results+=("${path}|${reason}")
  fi
}

# Baseline docs
add_doc "README.md" "Docs index"
add_doc "CODEX_RULES.md" "Project coding and safety rules"

# Emergency and protocol keywords
if [[ "$q" == *"em"* || "$q" == *"emergency"* || "$q" == *"302.1"* || "$q" == *"sos"* || "$q" == *"safe"* || "$q" == *"need"* || "$q" == *"resource"* || "$q" == *"heartbeat"* ]]; then
  add_doc "REF_EmergencyMode.md" "Emergency mode behavior and UI rules"
  add_doc "REF_3021.md" "302.1 packet-level mapping"
  add_doc "HermesX_EM_UI_v2.1.md" "Current EM UI interaction contract"
  add_doc "REF_EMACT_lite.md" "Emergency transmit lock behavior"
fi

# Input and power keywords
if [[ "$q" == *"button"* || "$q" == *"rotary"* || "$q" == *"gpio"* || "$q" == *"power"* || "$q" == *"sleep"* || "$q" == *"wake"* ]]; then
  add_doc "REF_techspec.md" "Input and power integration contract"
  add_doc "ISSUE_powerhold_longpress.md" "Known long-press failure notes"
fi

# LED and feedback keywords
if [[ "$q" == *"led"* || "$q" == *"ack"* || "$q" == *"nack"* || "$q" == *"buzzer"* || "$q" == *"animation"* ]]; then
  add_doc "REF_prd.md" "Acceptance criteria for LED and messaging behavior"
  add_doc "REF_LED_central_manager.md" "LED priority and trigger ownership"
fi

# UI orientation keywords
if [[ "$q" == *"ui"* || "$q" == *"screen"* || "$q" == *"display"* || "$q" == *"tft"* || "$q" == *"portrait"* ]]; then
  add_doc "UI_orientation_notes.md" "UI orientation strategy"
  add_doc "TFT_portrait_attempts.md" "Display rotation implementation history"
fi

# Release and status keywords
if [[ "$q" == *"status"* || "$q" == *"release"* || "$q" == *"version"* || "$q" == *"changelog"* || "$q" == *"regression"* ]]; then
  add_doc "REF_status.md" "Project state and known issues"
  add_doc "CHANGELOG.md" "Detailed timeline of behavior changes"
  add_doc "CHANGELOG_MINI.md" "Condensed timeline"
  add_doc "REF_changelog.md" "Changelog comparison format"
fi

# Fallback for generic implementation requests
if [[ ${#results[@]} -le 2 ]]; then
  add_doc "REF_prd.md" "Primary requirements baseline"
  add_doc "REF_techspec.md" "Architecture and behavior baseline"
fi

echo "Recommended docs for: ${query}"
printf '%-72s | %s\n' "Path" "Reason"
printf '%s\n' "------------------------------------------------------------------------|------------------------------------------"
printf '%s\n' "${results[@]}" | awk -F'|' '!seen[$1]++ {printf "%-72s | %s\n", $1, $2}'
