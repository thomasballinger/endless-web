# AI testing tools

The AI that controls non-player ships runs from rule lists in game data
(`data/ai.txt`, loaded by `AIRules`), rather than from a hard-coded chain of
`if` statements. These tools check that changes to the AI, or to the rules,
do exactly what is intended.

## Determinism

With `--rngseed N`, two runs of the same build and data produce exactly the
same simulation. That requires:

- a single random number generator shared by all threads (`Random.cpp`), and
- AI decisions that don't depend on where objects are in memory (for example,
  `governmentRosters` is sorted by government name, not by address).

## Debug environment variables

- `ES_STATE_HASH=1`: every 60 steps, print `STATEHASH <step> ships=<n> <hash>`,
  a hash of every ship's position, velocity and facing.
- `ES_STATE_DETAIL=<first>-<last>` (with `ES_STATE_HASH`): on each step in the
  range, print every ship's state and target, and the order governments are
  iterated in. Use this to find exactly where two runs diverge.
- `ES_AI_RULE_STATS=1`: at exit, print how often each AI rule ran, and how often
  it ended a ship's turn (or, for movement rules, moved the ship). Useful for
  seeing which rules a scenario exercises.

## compare-runs.sh

Runs an integration test scenario under several variants and checks that the
state hashes match:

```sh
# Is the simulation reproducible?
tests/ai/compare-runs.sh build/endless-sky "AI Scenario - Sol traffic"

# Does a refactor preserve behavior? Compare against a build of the previous commit.
tests/ai/compare-runs.sh build/endless-sky "AI Scenario - Ne-20 Avgi patrols" \
	"before:BINARY=/path/to/old/endless-sky" "after"

# Does a plugin change behavior, and which rules ran?
ES_KEEP_DIR=/tmp/demo tests/ai/compare-runs.sh build/endless-sky \
	"AI Scenario - Al Dhanab carriers and pirates" \
	"default:ES_AI_RULE_STATS=1" "loyal:PLUGIN=tests/ai/plugins/loyal-escorts ES_AI_RULE_STATS=1"
```

The scenarios, in `tests/integration/.../tests_ai_scenarios.txt`, leave the
player idle for about 12,000 steps:

- **Sol traffic**: escorts and heavy civilian traffic.
- **Al Dhanab carriers and pirates**: carriers, fighters, pirates, merchants
  and miners; covers combat, disabled ships, mining, harvesting and fighter
  recall.
- **Ne-20 Avgi patrols**: surveillance, secretive and swarming ships.

## plugins/

- `loyal-escorts`: overrides the movement rules for the player's escorts so
  they stay with the flagship instead of chasing enemies.
