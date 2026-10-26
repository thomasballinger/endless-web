#!/usr/bin/env bash
# Copyright (c) 2026 by Thomas Ballinger
#
# Endless Sky is free software: you can redistribute it and/or modify it under the
# terms of the GNU General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option) any later version.
#
# Endless Sky is distributed in the hope that it will be useful, but WITHOUT ANY
# WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
# PARTICULAR PURPOSE. See the GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along with
# this program. If not, see <https://www.gnu.org/licenses/>.

# Record a video of an integration test scenario, as the game draws it.
#
# Usage: record.sh BINARY "TEST NAME" OUTPUT.mp4 [PLUGIN_DIR...]
#
# The game runs with --debug, which makes test scenarios open a window and
# play in real time, on its own virtual X display. The view is zoomed all the
# way out. ES_STATE_HASH output is saved next to the video (OUTPUT.hashes), so
# the recording can be checked against headless runs of the same scenario.
#
# Environment:
#   ES_RESOURCES    resource directory (default: the repository root)
#   ES_SEED         value for --rngseed (default: 42)
#   ES_VIEW_ZOOM    "view zoom" preference; 0 is the widest view (default: 0)

set -u

if [ $# -lt 3 ]; then
	sed -n '/^# Usage/,/^# Environment/p' "$0" | sed 's/^# \{0,1\}//'
	exit 2
fi

BINARY=$(realpath "$1")
TEST_NAME=$2
OUTPUT=$(realpath -m "$3")
shift 3
PLUGINS=("$@")

REPO=$(cd "$(dirname "$0")/../.." && pwd)
RESOURCES=${ES_RESOURCES:-$REPO}
SEED=${ES_SEED:-42}
WORK=$(mktemp -d)
CONFIG="$WORK/config"
cp -r "$REPO/tests/integration/config" "$CONFIG"
for plugin in "${PLUGINS[@]}"; do
	cp -r "$plugin" "$CONFIG/plugins/"
done
grep -v '^"view zoom"' "$CONFIG/preferences.txt" > "$WORK/preferences.txt"
echo "\"view zoom\" ${ES_VIEW_ZOOM:-0}" >> "$WORK/preferences.txt"
mv "$WORK/preferences.txt" "$CONFIG/preferences.txt"

# Find a free display number.
DISPLAY_NUMBER=100
while [ -e "/tmp/.X11-unix/X$DISPLAY_NUMBER" ] || [ -e "/tmp/.X$DISPLAY_NUMBER-lock" ]; do
	DISPLAY_NUMBER=$((DISPLAY_NUMBER + 1))
done
Xvfb ":$DISPLAY_NUMBER" -screen 0 1280x720x24 > /dev/null 2>&1 &
XVFB=$!
sleep 1

# The game window is 1180x620, centered on the virtual screen.
DISPLAY=":$DISPLAY_NUMBER" ffmpeg -loglevel error -y -f x11grab -framerate 30 -video_size 1280x720 \
	-i ":$DISPLAY_NUMBER" -vf "crop=1180:620:50:50" -c:v libx264 -preset veryfast -crf 26 \
	-pix_fmt yuv420p "$OUTPUT" &
FFMPEG=$!

DISPLAY=":$DISPLAY_NUMBER" ES_STATE_HASH=1 "$BINARY" --debug --config "$CONFIG" --resources "$RESOURCES" \
	--rngseed "$SEED" --test "$TEST_NAME" > "$WORK/game.log" 2>&1
STATUS=$?

kill -INT "$FFMPEG"
wait "$FFMPEG" 2> /dev/null
kill "$XVFB"
grep '^STATEHASH' "$WORK/game.log" > "${OUTPUT%.*}.hashes"
echo "$TEST_NAME: exit $STATUS, $(wc -l < "${OUTPUT%.*}.hashes") hashes, video $OUTPUT"
rm -rf "$WORK"
exit $STATUS
