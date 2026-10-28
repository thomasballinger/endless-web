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

# Check that editing a script file while the game runs takes effect: run a
# scenario twice with --watch-scripts behavior on (ES_WATCH_SCRIPTS=1), and
# partway through one run, apply an edit to its copy of a plugin's file. The
# runs should match until the edit is reloaded, and then differ only once the
# edited code does something different. A script with errors is not reloaded,
# so with a broken edit the runs should match all the way through.
#
# Usage: live-edit.sh BINARY "TEST NAME" PLUGIN_DIR FILE SED_EXPRESSION [AFTER]
#
# PLUGIN_DIR is a plugin directory, FILE a path inside it (e.g.
# data/guidance.txt), and SED_EXPRESSION the edit, as for "sed -e". The edit
# is made once the edited run has printed AFTER state hashes (default 60;
# output is buffered, so the game may be some way past that point).
#
# Environment: ES_SEED (default 42) and ES_KEEP_DIR, as for compare-runs.sh.

set -u

if [ $# -lt 5 ]; then
	sed -n '/^# Usage/,/^# Environment/p' "$0" | sed 's/^# \{0,1\}//'
	exit 2
fi

BINARY=$(realpath "$1")
TEST_NAME=$2
PLUGIN=$(realpath "$3")
FILE=$4
EDIT=$5
AFTER=${6:-60}

HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${ES_KEEP_DIR:-$(mktemp -d)}
mkdir -p "$OUT"
NAME=$(basename "$PLUGIN")

ES_KEEP_DIR="$OUT" "$HERE/compare-runs.sh" "$BINARY" "$TEST_NAME" \
	"unedited:ES_WATCH_SCRIPTS=1 PLUGIN=$PLUGIN" \
	"edited:ES_WATCH_SCRIPTS=1 PLUGIN=$PLUGIN" > "$OUT/compare.txt" 2>&1 &
RUNS=$!

# compare-runs.sh copies the plugin into each run's configuration directory.
TARGET="$OUT/config-edited/plugins/$NAME/$FILE"
until [ -f "$OUT/edited.log" ] && [ "$(grep -c '^STATEHASH' "$OUT/edited.log")" -ge "$AFTER" ]; do
	if ! kill -0 $RUNS 2> /dev/null; then
		echo "The runs ended before the edit could be made."
		cat "$OUT/compare.txt"
		exit 1
	fi
	sleep 0.2
done
cp "$TARGET" "$OUT/before-edit.txt"
sed -i -e "$EDIT" "$TARGET"
if cmp -s "$TARGET" "$OUT/before-edit.txt"; then
	echo "The edit didn't change $FILE."
	wait $RUNS
	exit 1
fi
wait $RUNS

# The step of the last state hash before the reload message.
RELOAD=$(grep -E '^STATEHASH|Reloaded|Errors in' "$OUT/edited.log" \
	| awk '/^STATEHASH/{step=$2} /Reloaded|Errors in/{print step; exit}')
grep -hE 'Reloaded|Errors in' "$OUT/edited.log" | sed 's/^.*| I | //'
echo "Reloaded after step ${RELOAD:-?}."
FIRST=$(diff "$OUT/unedited.hashes" "$OUT/edited.hashes" | awk '/^</{print $3; exit}')
if [ -z "$FIRST" ]; then
	echo "The runs never differed."
else
	echo "The runs first differ at step $FIRST."
fi
echo "Logs and hashes in $OUT"
