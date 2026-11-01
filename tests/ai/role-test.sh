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

# Check that the player's escorts do what their roles say. Depart into the
# Al Dhanab scenario, where (in this test) pirate Ravens come to attack, with
# a role given to all of the player's escorts, and check that the escorts
# do what it says: for most roles, keep station until the pirates come, then
# attack them.
#
# Usage: role-test.sh BINARY [ROLE]
#
# ROLE is a role in data/roles.txt (by default, "Bodyguard").
# Needs Xvfb (xvfb-run). Environment: ES_KEEP_DIR, a directory to keep the
# game's output in.

set -u

if [ $# -lt 1 ]; then
	sed -n '/^# Usage/,/^# Environment/p' "$0" | sed 's/^# \{0,1\}//'
	exit 2
fi

BINARY=$(realpath "$1")
ROLE=${2:-Bodyguard}
HERE=$(cd "$(dirname "$0")" && pwd)
ROOT=$(cd "$HERE/../.." && pwd)
OUT=${ES_KEEP_DIR:-$(mktemp -d)}
mkdir -p "$OUT"
cp -r "$ROOT/tests/integration/config" "$OUT/config"

TESTS="$OUT/config/plugins/integration-tests/data/tests"
"$HERE/add-outfits.py" "$TESTS/tests_ai_scenarios.txt" "AI Combat Carriers Save" "Role Save" > "$TESTS/tests_roles.txt"
# Give every escort the role.
python3 - "$TESTS/tests_roles.txt" "$ROOT/data/roles.txt" "$ROLE" << 'PYTHON'
import sys
path, roles, role = sys.argv[1:]
lines = open(roles).read().split("\n")
start = lines.index('role "%s"' % role)
end = start + 1
while end < len(lines) and (lines[end].startswith("\t") or not lines[end]):
    end += 1
while not lines[end - 1]:
    end -= 1
body = "".join("\t\t\tline `%s`\n" % line for line in lines[start:end])
text = open(path).read()
text = text.replace("\t\tplanet Burthen\n", "\t\tplanet Burthen\n\t\t\"escort role\" *\n" + body, 1)
open(path, "w").write(text)
PYTHON
cat >> "$TESTS/tests_roles.txt" << EOF
test "Escort roles"
	status active
	description "Depart Burthen with roles for the escorts, and wait while pirates attack."
	sequence
		inject "Role Save"
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

fleet "Role Test Raiders"
	government "Pirate"
	names "pirate"
	personality
		heroic vindictive staying nemesis
	variant
		"Raven" 2

system "Al Dhanab"
	add fleet "Role Test Raiders" 300
EOF

RUNNER=()
if [ -z "${DISPLAY:-}" ]; then
	RUNNER=(xvfb-run -a)
fi
ES_LOG_ROLES=1 "${RUNNER[@]}" "$BINARY" --config "$OUT/config" --resources "$ROOT" --rngseed 1 \
	--test "Escort roles" > "$OUT/game.log" 2>&1
echo "game exited with status $?"

FAILED=0
check() {
	local count
	count=$(grep -c "$2" "$OUT/game.log")
	if [ "$count" -gt 0 ]; then
		echo "ok: $1 ($count times)"
	else
		echo "FAILED: $1"
		FAILED=1
	fi
}
check "the role runs and decides" "^ROLE "
if grep -q "^ROLE .*: failed" "$OUT/game.log"; then
	echo "FAILED: a role failed: $(grep -m1 '^ROLE .*: failed' "$OUT/game.log")"
	FAILED=1
fi
# What each role should be seen doing.
case "$ROLE" in
	Shieldwall)
		check "escorts keep station between the leader and the pirates" "^ROLE $ROLE .*: keep station";;
	"Wolf Pack")
		check "escorts leave it to the usual AI" "^ROLE $ROLE .*: independent"
		check "and pile onto hurt ships" "^ROLE $ROLE .*: attack";;
	"Hit and Run")
		check "escorts strike" "^ROLE $ROLE .*: attack threat"
		check "and fall back" "^ROLE $ROLE .*: retreat";;
	*)
		check "escorts keep station" "^ROLE $ROLE .*: keep station"
		check "and attack the pirates when they come" "^ROLE $ROLE .*: attack";;
esac
echo "Output in $OUT"
exit $FAILED
