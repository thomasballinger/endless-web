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
#   2. Coasting sooner makes a difference only once a missile is nearly lined
#      up with the jinking target, which is partway into the drill, so the
#      battle rewinds to there and goes on differently.
#   3. Saving writes the script into a plugin.
#   4. One undo takes back the whole edit, which makes a difference at that
#      same step: the runs were the same until then.
#   5. Fired at a Carrier, whose radar jammers confuse the missiles' seekers,
#      a confused missile's trail is marked, and selecting it makes the code
#      glitch, saying that the seeker is steering instead of the script.
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

# Open the simulator, with the given configuration, firing at the given ship.
start() {
	ES_SIMULATOR_LOG=1 "$BINARY" --config "$1" --resources "$ROOT" \
		--simulator "Sidewinder Missile Launcher" --simulator-target "$2" >> "$OUT/game.log" 2>&1 &
	GAME=$!
	WINDOW=
	for i in $(seq 100); do
		WINDOW=$(xdotool search --name "Endless Sky" 2> /dev/null | head -1)
		[ -n "$WINDOW" ] && break
		sleep 0.2
	done
	# With no window manager, the game thinks its window fills the screen.
	xdotool windowmove "$WINDOW" 0 0 windowsize "$WINDOW" 1600 1000
	for i in $(seq 120); do
		shot start
		"$LOOK" ready "$OUT/start.png" && break
		sleep 0.5
	done
}

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

start "$OUT/config" Argosy
check "the simulator starts" ready "$OUT/start.png" || exit 1

# The jinking drill, at 2x speed for a few seconds (not long enough for the
# drill to start over), then paused.
xdotool key 5 equal
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
# What the edits that compiled did, as the game reports them.
edits() { grep -c '^SIMULATOR edit' "$OUT/game.log"; }
EDITS=$(edits)

# Line 24 is "if (> (* steps-to-face 1.5) steps-to-reach)"; coast sooner.
xdotool mousemove 600 "$(line 24)" click 1
xdotool key End shift+Home
xdotool type --delay 20 "if (> (* steps-to-face 3) steps-to-reach)"
# Let the rewind finish.
sleep 2
shot after
check "the edit replays the battle differently" differ "$OUT/before.png" "$OUT/after.png"
# Typing the line changes it several times; the last edit is the one that counts.
EDIT=$(grep '^SIMULATOR edit' "$OUT/game.log" | tail -1)
echo "  $EDIT"
NOW=$(echo "$EDIT" | sed -n 's/^SIMULATOR edit at step \([0-9]*\):.*/\1/p')
FIRST=$(echo "$EDIT" | sed -n 's/.*first difference at step \([0-9]*\).*/\1/p')
if [ -n "$FIRST" ] && [ "$(edits)" -gt "$EDITS" ] && [ "$FIRST" -gt 1 ] && [ "$FIRST" -lt "$NOW" ]; then
	echo "ok: the edit first makes a difference partway into the drill"
else
	echo "FAILED: the edit first makes a difference partway into the drill"
	FAILED=1
fi

xdotool key ctrl+s
sleep 0.5
SAVED="$OUT/config/plugins/simulator scripts/data/guidance leading and throttle control.txt"
if grep -q "if (> (\* steps-to-face 3) steps-to-reach)" "$SAVED" 2> /dev/null; then
	echo "ok: saving writes the edited script"
else
	echo "FAILED: saving writes the edited script"
	FAILED=1
fi

EDITS=$(edits)
xdotool key ctrl+z
sleep 2
UNDO=$(grep '^SIMULATOR edit' "$OUT/game.log" | tail -1)
echo "  $UNDO"
if [ "$(edits)" -eq $((EDITS + 1)) ] && echo "$UNDO" | grep -q "first difference at step $FIRST,"; then
	echo "ok: one undo takes the edit back, with a difference at the same step"
else
	echo "FAILED: one undo takes the edit back, with a difference at the same step"
	FAILED=1
fi

kill $GAME
wait $GAME 2> /dev/null

# The jinking drill again, but against jammers. Wait for a confused missile,
# pause, and select it. (A new configuration leaves out the script saved above.)
cp -r "$ROOT/tests/integration/config" "$OUT/config-jammed"
start "$OUT/config-jammed" Carrier
xdotool key 5 equal equal
WHERE=
for i in $(seq 60); do
	shot jammed
	WHERE=$("$LOOK" confused "$OUT/jammed.png") && break
	sleep 0.3
done
xdotool key space
sleep 0.5
shot jammed
if WHERE=$("$LOOK" confused "$OUT/jammed.png"); then
	echo "ok: a confused missile's trail is marked"
	xdotool mousemove $WHERE click 1
	sleep 0.5
	shot glitch
	check "selecting a confused missile makes the code glitch" glitch "$OUT/glitch.png"
else
	echo "FAILED: a confused missile's trail is marked"
	FAILED=1
fi

echo "Screenshots in $OUT"
exit $FAILED
