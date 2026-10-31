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

# Check, without anyone at the keyboard, that the player can program their
# missiles in the missile workshop. On a virtual display (Xvfb), load the Al
# Dhanab scenario's saved game, landed on Burthen, whose flagship carries a
# Meteor Missile Launcher; then:
#
#   1. The planet's Workshop button (w) opens the workshop.
#   2. Choosing the first weapon (1) opens it in the simulator.
#   3. Adding a line to its program and installing it (ctrl+s) makes the
#      workshop list it as the player's program.
#   4. Taking off saves the program with the game.
#   5. In flight, the flagship's missiles fly it: its code, in the pane of
#      the flagship's scripts, shows the added line running.
#
# Usage: workshop-ui.sh BINARY
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

# The scenario's saved game, and a test that loads it and waits, landed.
TESTS="$OUT/config/plugins/integration-tests/data/tests"
"$HERE/add-outfits.py" "$TESTS/tests_ai_scenarios.txt" "AI Combat Carriers Save" "Workshop Save" \
	> "$TESTS/tests_workshop_ui.txt"
cat >> "$TESTS/tests_workshop_ui.txt" << EOF
test "Missile workshop"
	status active
	description "Load a game landed on Burthen, and wait."
	sequence
		inject "Workshop Save"
		call "Load First Savegame"
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
"$BINARY" --config "$OUT/config" --resources "$ROOT" --test "Missile workshop" --debug > "$OUT/game.log" 2>&1 &
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
key() {
	xdotool key "$@"
	sleep 1
}

# Wait for the game to load the saved game (one test step every 30 frames).
sleep 16
key w
shot workshop
check "the planet's Workshop button opens the workshop" workshop "$OUT/workshop.png"

key 1
sleep 1
shot simulator
check "choosing a weapon opens it in the simulator" ready "$OUT/simulator.png"

# Turn half as hard: add a line at the end of the program, and install it.
key e
key ctrl+End Return
xdotool type --delay 30 "turn = turn * .5"
sleep 1
key ctrl+s
shot installed
key Escape
key Escape
shot programmed
check "installing the program makes the workshop list it as the player's" programmed "$OUT/programmed.png"

# The escort workshop: hold station farther out (line 4 is the position
# deadband, 200), and install that for the player's escorts.
key e
shot escorts
check "the workshop opens the escort workshop" escorts "$OUT/escorts.png"
xdotool mousemove 300 112 click 1
key End BackSpace BackSpace BackSpace
xdotool type --delay 30 "600"
sleep 1
key ctrl+s
key Escape
key Escape

# Take off, which saves the game.
key Escape
key d
sleep 3
SAVE=$(grep -l "guidance program" "$OUT"/config/saves/*.txt 2> /dev/null | head -1)
if [ -n "$SAVE" ] && grep -q "turn = turn \* .5" "$SAVE"; then
	echo "ok: taking off saves the program with the game"
else
	echo "FAILED: taking off saves the program with the game"
	FAILED=1
fi
if [ -n "$SAVE" ] && grep -q "maneuver program" "$SAVE" && grep -q "let position-deadband 600" "$SAVE"; then
	echo "ok: and the escorts' program"
else
	echo "FAILED: and the escorts' program"
	FAILED=1
fi

# In flight, the flagship's missiles fly the program: fire one, and open the
# flagship's code, where line 7 (the line added above, which runs while the
# missile has no target) is marked as having run.
hold() {
	xdotool keydown "$1"
	sleep "$2"
	xdotool keyup "$1"
	sleep 0.3
}
sleep 2
hold w 0.15
hold w 0.15
hold q 0.6
hold k 0.15
sleep 0.5
shot flying
check "the flagship's missiles fly the program" lit "$OUT/flying.png" 300 278 312 296

echo "Screenshots in $OUT"
exit $FAILED
