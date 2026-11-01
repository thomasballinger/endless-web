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

# Check that defense daemons hack the missiles chasing the ships that run
# them. Depart into the Al Dhanab scenario, where (in this test) pirate
# Ravens come to attack the player's fleet, with a Defense Daemon (and a code scanner and injector) on the
# flagship, and with a test plugin that has the pirates run a daemon of their
# own; with faction scripts on, both should hack missiles:
#
#   1. The flagship's daemon hacks the missiles chasing it, or its escorts.
#   2. The pirates' daemon hacks the missiles chasing them.
#
# Usage: daemon-test.sh BINARY
#
# Needs Xvfb (xvfb-run). Environment: ES_KEEP_DIR, a directory to keep the
# game's output in.

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

TESTS="$OUT/config/plugins/integration-tests/data/tests"
"$HERE/add-outfits.py" "$TESTS/tests_ai_scenarios.txt" "AI Combat Carriers Save" "Daemon Save" \
	"Code Scanner:16" "Code Injector" "Defense Daemon" "LP144a Battery Pack:4" > "$TESTS/tests_daemon.txt"
cat >> "$TESTS/tests_daemon.txt" << EOF
test "Defense daemons"
	status active
	description "Depart Burthen with a defense daemon, and wait while pirates attack."
	sequence
		inject "Daemon Save"
		call "Load First Savegame"
		call "Depart"
		apply
			"test: steps to wait" = 2400
		label "idle"
		input
		apply
			"test: steps to wait" = "test: steps to wait" - 1
		branch "idle"
			"test: steps to wait" > 0

program "Test Pirate Daemon"
	daemon
	author "Pirate"
	"flown by" "Pirate"
	description "Hacks the nearest missile."

daemon "Test Pirate Daemon"
	if hacked == 0 && steps_away < 300
		score = 1000 - steps_away

# Ravens, which carry Sidewinder missiles and torpedoes, come to fight.
fleet "Daemon Test Ambush"
	government "Pirate"
	names "pirate"
	personality
		heroic vindictive staying
	variant
		"Raven" 2

system "Al Dhanab"
	add fleet "Daemon Test Ambush" 300
EOF

RUNNER=()
if [ -z "${DISPLAY:-}" ]; then
	RUNNER=(xvfb-run -a)
fi
ES_LOG_DAEMONS=1 ES_FACTION_MANEUVERS=1 "${RUNNER[@]}" "$BINARY" --config "$OUT/config" --resources "$ROOT" \
	--rngseed 1 --test "Defense daemons" > "$OUT/game.log" 2>&1
echo "game exited with status $?"

FAILED=0
check() {
	local count
	count=$(grep -c "$2" "$OUT/game.log")
	if [ "$count" -gt 0 ]; then
		echo "ok: $1 ($count hacks)"
	else
		echo "FAILED: $1"
		FAILED=1
	fi
}
check "the flagship's daemon hacks the missiles chasing the fleet" "^DAEMON [0-9]* flagship nearest first hacked"
check "the pirates' daemon hacks the missiles chasing them" "^DAEMON [0-9]* Pirate Test Pirate Daemon hacked"
echo "Output in $OUT"
exit $FAILED
