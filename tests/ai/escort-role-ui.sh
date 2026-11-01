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

# Check, without anyone at the keyboard, that the escort workshop can try out
# an escort's role against a raider. On a virtual display (Xvfb), open the
# escort workshop on a role (Bodyguard, to start with), and check that:
#
#   1. It opens.
#   2. A raider comes, and the escort, as its role says, fights it off.
#   3. Changing the role so that the escort only ever keeps station (its
#      "attack_threat" line becomes "keep_station"), the escort lets raiders
#      be: none is destroyed in the replay.
#
# Usage: escort-role-ui.sh BINARY
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
rm -rf "$OUT/config"
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

ES_SIMULATOR_LOG=1 "$BINARY" --config "$OUT/config" --resources "$ROOT" --simulator role \
	--simulator-target Sparrow > "$OUT/game.log" 2>&1 &
GAME=$!
WINDOW=
for i in $(seq 100); do
	WINDOW=$(xdotool search --name "Endless Sky" 2> /dev/null | head -1)
	[ -n "$WINDOW" ] && break
	sleep 0.2
done
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
key() {
	xdotool key --delay 80 "$@"
	sleep 0.3
}

for i in $(seq 120); do
	shot start
	"$LOOK" escorts "$OUT/start.png" && break
	sleep 0.5
done
check "the escort workshop opens on a role" escorts "$OUT/start.png"

# The first raider comes about four seconds in; give the escort time to fight.
sleep 20
shot fight
if grep -q "raider destroyed" "$OUT/game.log"; then
	echo "ok: the escort fights off the raider ($(grep -c 'raider destroyed' "$OUT/game.log") destroyed)"
else
	echo "FAILED: the escort fights off the raider"
	FAILED=1
fi

# Line 12 of Bodyguard is "attack_threat": make it "keep_station" instead,
# and start over, so the escort never fights.
key e
key ctrl+Home
for i in $(seq 11); do key Down; done
key End shift+Home
xdotool type --delay 30 "		keep_station"
sleep 2
key Escape
BEFORE=$(grep -c "raider destroyed" "$OUT/game.log")
key r
sleep 20
shot passive
AFTER=$(grep -c "raider destroyed" "$OUT/game.log")
if [ "$AFTER" -eq "$BEFORE" ]; then
	echo "ok: with the attack taken out of the role, the escort lets the raider be"
else
	echo "FAILED: with the attack taken out of the role, the escort lets the raider be ($((AFTER - BEFORE)) destroyed)"
	FAILED=1
fi

echo "Screenshots in $OUT"
exit $FAILED
