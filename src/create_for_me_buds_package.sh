#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1 && pwd)"
CONFIG_FILE="$SCRIPT_DIR/config/seb01_earbuds/sndp_config.mk"

if [[ ! -f "$CONFIG_FILE" ]]; then
  echo "ERROR: config file not found: $CONFIG_FILE" >&2
  exit 1
fi

SOFTWARE_VERSION="$(grep -E '^[[:space:]]*(export[[:space:]]+)?SOFTWARE_VERSION[[:space:]]*\?=' "$CONFIG_FILE" | sed -E 's/^[[:space:]]*(export[[:space:]]+)?SOFTWARE_VERSION[[:space:]]*\?=[[:space:]]*"?(.*?)"?$/\2/' | head -n 1 | tr -d '\r')"

if [ -z "$SOFTWARE_VERSION" ]; then
  echo "ERROR: SOFTWARE_VERSION not found in $CONFIG_FILE" >&2
  exit 2
fi

TARGET_BASE="$SCRIPT_DIR/For_me_buds_${SOFTWARE_VERSION}"
BOX_DIR="$TARGET_BASE/box"
EARBUDS_DIR="$TARGET_BASE/earbuds"
DOWNLOAD_DIR="$EARBUDS_DIR/download"
OTA_DIR="$EARBUDS_DIR/ota"
README_FILE="$TARGET_BASE/Readme.txt"

mkdir -p "$BOX_DIR" "$DOWNLOAD_DIR" "$OTA_DIR"

echo "Created directory structure:"
echo "  $BOX_DIR"
echo "  $DOWNLOAD_DIR"
echo "  $OTA_DIR"

version_decrement() {
  local version="$1"
  IFS='.' read -ra parts <<< "$version"
  local idx=$(( ${#parts[@]} - 1 ))
  if [[ $idx -lt 0 ]]; then
    return 1
  fi

  if [[ "${parts[idx]}" =~ ^[0-9]+$ ]]; then
    if [[ ${parts[idx]} -gt 0 ]]; then
      parts[idx]=$((parts[idx] - 1))
    else
      echo "ERROR: cannot decrement version $version with trailing zero" >&2
      return 1
    fi
  else
    echo "ERROR: invalid numeric version segment: ${parts[idx]}" >&2
    return 1
  fi

  local result="${parts[0]}"
  for i in "${parts[@]:1}"; do
    result+=".$i"
  done
  printf '%s' "$result"
}

write_readme() {
  local prev_version
  prev_version="$(version_decrement "$SOFTWARE_VERSION")"
  local prev_marker="V${prev_version}"
  local current_marker="V${SOFTWARE_VERSION}"
  local last_commit_id=""
  local commit_lines=""
  local current_commit=""
  local prev_commit=""

  {
    echo "Commit messages from ${prev_marker} to ${current_marker}:"
    echo
  } > "$README_FILE"

  if git rev-parse --git-dir >/dev/null 2>&1; then
    current_commit="$(git log --grep="$current_marker" --format='%H' -n 1 2>/dev/null || true)"
    prev_commit="$(git log --grep="$prev_marker" --format='%H' -n 1 2>/dev/null || true)"

    if [[ -n "$prev_commit" && -n "$current_commit" ]]; then
      commit_lines="$(git log --no-merges --reverse --pretty=format:'%h %s' "${prev_commit}..${current_commit}" 2>/dev/null || true)"
      last_commit_id="${current_commit:-$(git rev-parse HEAD 2>/dev/null || echo 'unknown')}"
      if [[ -z "$commit_lines" ]]; then
        commit_lines="[No commit messages found between ${prev_marker} and ${current_marker}.]"
      fi
    elif [[ -n "$prev_commit" ]]; then
      echo "WARNING: found previous version commit ${prev_marker} but not current version commit ${current_marker}; using HEAD as end point." >&2
      commit_lines="$(git log --no-merges --reverse --pretty=format:'%h %s' "${prev_commit}..HEAD" 2>/dev/null || true)"
      last_commit_id="$(git rev-parse HEAD 2>/dev/null || echo 'unknown')"
      if [[ -z "$commit_lines" ]]; then
        commit_lines="[No commit messages found between ${prev_marker} and HEAD.]"
      fi
    elif [[ -n "$current_commit" ]]; then
      echo "WARNING: found current version commit ${current_marker} but not previous version commit ${prev_marker}." >&2
      commit_lines="$(git log --no-merges --reverse --pretty=format:'%h %s' "${current_commit}..HEAD" 2>/dev/null || true)"
      last_commit_id="$(git rev-parse HEAD 2>/dev/null || echo 'unknown')"
      if [[ -z "$commit_lines" ]]; then
        commit_lines="[No commit messages found after ${current_marker}.]"
      fi
    else
      echo "WARNING: could not find commit message markers ${prev_marker} and/or ${current_marker}." >&2
      commit_lines="[No commit message markers found for ${prev_marker} and/or ${current_marker}.]"
      last_commit_id="$(git rev-parse HEAD 2>/dev/null || echo 'unknown')"
    fi
  else
    echo "WARNING: not a git repository; cannot extract commit messages." >&2
    commit_lines="[No git repository available.]"
    last_commit_id="unknown"
  fi

  {
    if [[ -n "$commit_lines" ]]; then
      printf '%s\n' "$commit_lines"
    fi
    echo
    echo "Last commit id: $last_commit_id"
  } >> "$README_FILE"
}

write_readme

target_copy() {
  local pattern="$1"
  local dest="$2"
  shopt -s nullglob
  local files=( $pattern )
  shopt -u nullglob
  if [[ ${#files[@]} -eq 0 ]]; then
    echo "WARNING: no files found matching pattern: $pattern"
    return
  fi
  for f in "${files[@]}"; do
    if [[ -f "$f" ]]; then
      cp -v "$f" "$dest/"
    fi
  done
}

target_copy_rename() {
  local pattern="$1"
  local dest="$2"
  shopt -s nullglob
  local files=( $pattern )
  shopt -u nullglob
  if [[ ${#files[@]} -eq 0 ]]; then
    echo "WARNING: no files found matching pattern: $pattern"
    return
  fi
  for f in "${files[@]}"; do
    if [[ -f "$f" ]]; then
      local base="$(basename "$f")"
      local rename="${base/#seb01_earbuds_/For_me_buds_}"
      cp -v "$f" "$dest/$rename"
    fi
  done
}

pushd "$SCRIPT_DIR" >/dev/null

# Remove stale original filenames from previous runs
rm -f "$OTA_DIR"/seb01_earbuds_app_for_ota_v*.bin "$DOWNLOAD_DIR"/seb01_earbuds_app_v*.bin "$DOWNLOAD_DIR"/seb01_earbuds_boot_20260413.bin 2>/dev/null || true

target_copy_rename "seb01_earbuds_app_for_ota_v*.bin" "$OTA_DIR"
target_copy_rename "seb01_earbuds_app_v*.bin" "$DOWNLOAD_DIR"

BOOT_FILE="$SCRIPT_DIR/out/prod_test/ota_copy/ota_copy.bin"
if [[ -f "$BOOT_FILE" ]]; then
  cp -v "$BOOT_FILE" "$DOWNLOAD_DIR/For_me_buds_boot_20260413.bin"
else
  echo "WARNING: boot file not found: $BOOT_FILE"
fi

popd >/dev/null

echo "Done. Target directory: $TARGET_BASE"
