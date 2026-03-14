#!/bin/bash
# Generate gamedata-manifest.txt for the Emscripten web build.
# Lists all game data files with their sizes in the format:
#   size<TAB>relative_path
# Directories are listed with size -1.
#
# Usage: ./generate-web-manifest.sh [game_root_dir] [output_file]
#   game_root_dir: Path to the ITGmania root directory (default: .)
#   output_file:   Output manifest path (default: gamedata-manifest.txt)

set -euo pipefail

GAME_ROOT="${1:-.}"
OUTPUT="${2:-gamedata-manifest.txt}"

# Directories to include in the manifest
DIRS=("Data" "NoteSkins" "Themes")

{
    for dir in "${DIRS[@]}"; do
        src="$GAME_ROOT/$dir"
        if [ ! -d "$src" ]; then
            echo "Warning: $src not found, skipping" >&2
            continue
        fi

        # List directories
        find "$src" -type d | while read -r d; do
            rel="${d#$GAME_ROOT/}"
            echo "-1	$rel"
        done

        # List files with sizes
        find "$src" -type f | while read -r f; do
            rel="${f#$GAME_ROOT/}"
            size=$(stat -c%s "$f" 2>/dev/null || stat -f%z "$f" 2>/dev/null)
            echo "$size	$rel"
        done
    done
} | sort -t$'\t' -k2 > "$OUTPUT"

echo "Generated $OUTPUT with $(wc -l < "$OUTPUT") entries" >&2
