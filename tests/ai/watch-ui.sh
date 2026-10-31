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

# Check, without anyone at the keyboard, that the player can watch a ship's
# scripts run in the game. On a virtual display (Xvfb), depart into the Al
# Dhanab scenario and target ships until one is running a script; then:
#
#   1. The HUD's map of the target's code lights up the lines that ran.
#   2. Clicking the map opens a pane with the script's code, with the lines
#      that ran marked.
#   3. Clicking the pane's close box closes it.
#
# Usage: watch-ui.sh BINARY
#
# Needs Xvfb, xdotool, ImageMagick's "import", and Python with Pillow.
# Environment: ES_KEEP_DIR, a directory to keep the screenshots in.

set -u

if [ $# -lt 1 ]; then
	sed -n '/^# Usage/,/^# Environment/p' "$0" | sed 's/^# \{0,1\}//'
	exit 2
fi

BINARY=$(realpath "$1")
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT=${ES_KEEP_DIR:-$(mktemp -d)}
mkdir -p "$OUT"
cp -r "$ROOT/tests/integration/config" "$OUT/config"
LOOK="$HERE/simulator-ui.py"

# An integration test that departs and then waits, so there is time to look.
cat > "$OUT/config/plugins/integration-tests/data/tests/tests_watch_ui.txt" << EOF
test "Watch scripts"
	status active
	description "Depart Burthen into a system with pirates, merchants and miners, and wait."
	sequence
		inject "AI Combat Carriers Save"
		call "Load First Savegame"
		call "Depart"
		apply
			"test: steps to wait" = 100000
		label "wait"
		input
		apply
			"test: steps to wait" = "test: steps to wait" - 1
		branch "wait"
			"test: steps to wait" > 0
EOF

N=150
while [ -e "/tmp/.X11-unix/X$N" ] || [ -e "/tmp/.X$N-lock" ]; do N=$((N + 1)); done
Xvfb ":$N" -screen 0 1600x1000x24 > /dev/null 2>&1 &
XVFB=$!
export DISPLAY=":$N"
GAME=
trap 'kill $GAME $XVFB 2> /dev/null' EXIT
sleep 1

# With --debug, a test draws the game as it runs.
"$BINARY" --config "$OUT/config" --resources "$ROOT" --test "Watch scripts" --debug > "$OUT/game.log" 2>&1 &
GAME=$!
WINDOW=
for i in $(seq 100); do
	WINDOW=$(xdotool search --name "Endless Sky" 2> /dev/null | head -1)
	[ -n "$WINDOW" ] && break
	sleep 0.2
done
# With no window manager, the game thinks its window fills the screen.
xdotool windowmove "$WINDOW" 0 0 windowsize "$WINDOW" 1600 1000
xdotool windowfocus "$WINDOW" 2> /dev/null

shot() { import -window root "$OUT/$1.png"; }
FAILED=0
check() {
	if "$LOOK" "${@:2}"; then
		echo "ok: $1"
	else
		echo "FAILED: $1"
		FAILED=1
	fi
}

# Where the HUD maps the target's code (see "target code" in the "hud"
# interface): the part below its name.
MAP="5 491 145 517"

# Wait for the game to depart (one test step every 30 frames), then target
# ships ("Select next ship", the n key) until one is running a script.
sleep 20
for i in $(seq 12); do
	xdotool keydown n
	sleep 0.15
	xdotool keyup n
	sleep 1
	shot target
	"$LOOK" lit "$OUT/target.png" $MAP && break
done
check "the target's code map lights up the lines that ran" lit "$OUT/target.png" $MAP

xdotool mousemove 75 504 click 1
sleep 1
shot pane
check "clicking the map opens the code, marking the lines that ran" pane "$OUT/pane.png"

# The close box is in the pane's top right corner.
xdotool mousemove 896 84 click 1
sleep 1
shot closed
if "$LOOK" pane "$OUT/closed.png"; then
	echo "FAILED: the close box closes the pane"
	FAILED=1
else
	echo "ok: the close box closes the pane"
fi

echo "Screenshots in $OUT"
exit $FAILED
