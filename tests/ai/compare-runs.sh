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

# Run one integration test scenario several times with ES_STATE_HASH set and
# check that every run produced exactly the same sequence of state hashes.
#
# Usage: compare-runs.sh BINARY "TEST NAME" [VARIANT...]
#
# Each VARIANT is "label" or "label:VAR=value VAR2=value ...". Every variant is
# run once with its environment variables set, and its hashes are compared to
# the first variant's. A variant can set BINARY=path to run a different build,
# e.g. to compare a refactored AI against the previous commit, or
# RESOURCES=path to use different game data. With no
# variants, the scenario is simply run twice.
#
# Environment:
#   ES_RESOURCES    resource directory (default: the repository root)
#   ES_SEED         value for --rngseed (default: 42)
#   ES_EXTRA_ARGS   extra command line arguments for every run
#   ES_KEEP_DIR     directory to keep logs and hashes in (default: a temp dir)
#
# Runs happen in parallel under xvfb-run if no DISPLAY is available.

set -u

if [ $# -lt 2 ]; then
	sed -n '/^# Usage/,/^# Runs happen/p' "$0" | sed 's/^# \{0,1\}//'
	exit 2
fi

BINARY=$(realpath "$1")
TEST_NAME=$2
shift 2
VARIANTS=("$@")
if [ ${#VARIANTS[@]} -eq 0 ]; then
	VARIANTS=("first" "second")
fi

REPO=$(cd "$(dirname "$0")/../.." && pwd)
RESOURCES=${ES_RESOURCES:-$REPO}
SEED=${ES_SEED:-42}
OUT=${ES_KEEP_DIR:-$(mktemp -d)}
mkdir -p "$OUT"
CONFIG_TEMPLATE="$REPO/tests/integration/config"

RUNNER=()
if [ -z "${DISPLAY:-}" ]; then
	RUNNER=(xvfb-run -a)
fi

run_variant() {
	local label=${1%%:*}
	local envs=""
	if [[ "$1" == *:* ]]; then
		envs=${1#*:}
	fi
	local binary=$BINARY
	local resources=$RESOURCES
	local word
	for word in $envs; do
		if [[ "$word" == BINARY=* ]]; then
			binary=$(realpath "${word#BINARY=}")
			envs=${envs//$word/}
		elif [[ "$word" == RESOURCES=* ]]; then
			resources=${word#RESOURCES=}
			envs=${envs//$word/}
		fi
	done
	local config="$OUT/config-$label"
	rm -rf "$config"
	cp -r "$CONFIG_TEMPLATE" "$config"
	# shellcheck disable=SC2086
	env ES_STATE_HASH=1 $envs "${RUNNER[@]}" "$binary" --config "$config" --resources "$resources" \
		--rngseed "$SEED" ${ES_EXTRA_ARGS:-} --test "$TEST_NAME" > "$OUT/$label.log" 2>&1
	local status=$?
	grep '^STATEHASH' "$OUT/$label.log" > "$OUT/$label.hashes"
	echo "$label: exit $status, $(wc -l < "$OUT/$label.hashes") hashes"
}

for variant in "${VARIANTS[@]}"; do
	run_variant "$variant" &
done
wait

FIRST=${VARIANTS[0]%%:*}
RESULT=0
if [ ! -s "$OUT/$FIRST.hashes" ]; then
	echo "FAIL: $FIRST produced no hashes (see $OUT/$FIRST.log)"
	exit 1
fi
for variant in "${VARIANTS[@]:1}"; do
	label=${variant%%:*}
	if cmp -s "$OUT/$FIRST.hashes" "$OUT/$label.hashes"; then
		echo "SAME: $FIRST and $label"
	else
		echo "DIFFERENT: $FIRST and $label, first difference:"
		diff "$OUT/$FIRST.hashes" "$OUT/$label.hashes" | head -4
		RESULT=1
	fi
done
echo "Logs and hashes in $OUT"
exit $RESULT
