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

# Check the simulator's script editor without anyone at the keyboard: run the
# game on a virtual display, type into the editor, and look at screenshots.
#
#   1. Typing a mistake marks its line, and undo clears the mark.
#   2. Reversing the missile's turn replays the battle differently.
#   3. Saving writes the script into a plugin.
#   4. One undo takes back the whole edit, and the battle with it.
#
# Usage: simulator-ui.sh BINARY
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

N=150
while [ -e "/tmp/.X11-unix/X$N" ] || [ -e "/tmp/.X$N-lock" ]; do N=$((N + 1)); done
Xvfb ":$N" -screen 0 1600x1000x24 > /dev/null 2>&1 &
XVFB=$!
export DISPLAY=":$N"
GAME=
trap 'kill $GAME $XVFB 2> /dev/null' EXIT
sleep 1

"$BINARY" --config "$OUT/config" --resources "$ROOT" --simulator "Sidewinder Missile Launcher" \
	> "$OUT/game.log" 2>&1 &
GAME=$!
for i in $(seq 100); do
	WINDOW=$(xdotool search --name "Endless Sky" 2> /dev/null | head -1)
	[ -n "$WINDOW" ] && break
	sleep 0.2
done
# With no window manager, the game thinks its window fills the screen.
xdotool windowmove "$WINDOW" 0 0 windowsize "$WINDOW" 1600 1000

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
check_not() {
	if "$LOOK" "${@:2}"; then
		echo "FAILED: $1"
		FAILED=1
	else
		echo "ok: $1"
	fi
}
# The center of a line of the script.
line() { echo $((59 + ($1 - 1) * 18)); }

for i in $(seq 120); do
	shot start
	"$LOOK" ready "$OUT/start.png" && break
	sleep 0.5
done
check "the simulator starts" ready "$OUT/start.png" || exit 1

# The crossing drill, at 8x speed for a few seconds, then paused.
xdotool key 4 equal equal equal
sleep 5
xdotool key space
# Let the camera settle.
sleep 2
shot before

# Line 21 is "set turn desired-turn"; add a word to the end of it.
xdotool mousemove 600 "$(line 21)" click 1
xdotool type --delay 20 " oops"
sleep 0.5
shot mistake
check "a mistake is marked" error "$OUT/mistake.png" 21
xdotool key ctrl+z
sleep 0.5
shot undone
check_not "undo clears the mark" error "$OUT/undone.png" 21
check_not "the mistake doesn't change the battle" differ "$OUT/before.png" "$OUT/undone.png"

# Line 19 is "set turn (copysign turn desired-turn)"; turn the other way.
xdotool mousemove 600 "$(line 19)" click 1
xdotool key End shift+Home
xdotool type --delay 20 "set turn (- (copysign turn desired-turn))"
sleep 1
shot after
check "the edit replays the battle differently" differ "$OUT/before.png" "$OUT/after.png"

xdotool key ctrl+s
sleep 0.5
SAVED="$OUT/config/plugins/simulator scripts/data/guidance leading and throttle control.txt"
if grep -q "set turn (- (copysign turn desired-turn))" "$SAVED" 2> /dev/null; then
	echo "ok: saving writes the edited script"
else
	echo "FAILED: saving writes the edited script"
	FAILED=1
fi

xdotool key ctrl+z
sleep 1
shot reverted
check_not "one undo puts the battle back" differ "$OUT/before.png" "$OUT/reverted.png"

echo "Screenshots in $OUT"
exit $FAILED
