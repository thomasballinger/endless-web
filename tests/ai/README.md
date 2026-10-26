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

Two more things made runs differ only rarely, depending on timing and on
where the memory allocator happened to put things:

- Sound pitch was randomized with the simulation's random numbers, on the
  main thread, while the simulation runs on another. It now uses the
  cosmetic generator.
- Fleets choosing which system to arrive from, and ships choosing a system to
  travel to, picked from sets of systems ordered by memory address. They now
  pick from the systems in name order (`System::InNameOrder`).

To check for this kind of problem, compare normal runs with runs that use a
different memory layout, e.g. `"a1:MALLOC_ARENA_MAX=1"` or
`"notcache:GLIBC_TUNABLES=glibc.malloc.tcache_count=0"`. (One run of Ne-20
with `MALLOC_ARENA_MAX=1` still differed once in about twenty, so there may be
another, rarer case left.)

Headless runs (integration tests) and drawn runs (`--debug`, `record.sh`)
are also the same. Two things used to differ: without uploading images,
sprites had no frames, so nothing was animated and only the first frame's
collision mask was used, and they had no area, so optical tracking never got
a lock. `ImageSet::Upload` now keeps both.

## Random streams

With one sequence of random numbers, any change in what one object does shifts
the numbers every other object gets, so the whole simulation diverges within
a few dozen steps. With `ES_RANDOM_STREAMS=1`, every ship, projectile,
asteroid and piece of flotsam has its own small generator (`Random::Stream`),
and `Random::Scope` makes the random functions use it while that object acts.
A new object is seeded from the stream of whatever made it (a projectile from
the ship that fired it, a fighter from its carrier); arriving fleets get one
seed each from the main sequence whether or not they actually arrive. Effects
and drawing use a separate stream, so they never change the simulation. Without the variable, everything
uses the main sequence exactly as before.

Measured with `divergence.py` on the Al Dhanab scenario with the
pursuit-missiles plugin (12,000 steps; "differed" counts ships that were ever
in a different state, out of all ships seen in either run; "end" is the last
step's same / different / in only one run):

| Seed | One sequence: differed | end | Streams: differed | end |
|---|---|---|---|---|
| 1 | 322 of 325 | 3 / 3 / 71 | 119 of 210 | 29 / 4 / 1 |
| 2 | 257 of 283 | 3 / 2 / 33 | 126 of 203 | 3 / 19 / 8 |
| 3 | 79 of 220 | 5 / 5 / 53 | 90 of 160 | 5 / 15 / 14 |

With one sequence, the first difference changes which fleets arrive, so by
the end most ships exist in only one of the two runs. With streams, the same
fleets arrive, and a difference only spreads through what ships actually do
to each other. In a large battle that still reaches many ships (who is
targeted, who is damaged, which side is stronger), but in a quieter system
like Ne-20 it stays small (seed 42: 19 of 53 ships with streams, 68 of 85
without). Seeds are not comparable between the two columns: with streams,
every random number is different, so the battles are different.

`ES_RANDOM_DRAWS=1` (with `ES_STATE_HASH`) prints how many numbers have been
taken from the main sequence. With streams, that count should be the same in
both runs; if it is not, some code is taking numbers from the main sequence
in a way that depends on what an object did, and needs a `Random::Scope`.

## Debug environment variables

- `ES_STATE_HASH=1`: every 60 steps, print `STATEHASH <step> ships=<n> <hash>`,
  a hash of every ship's position, velocity and facing.
- `ES_STATE_DETAIL=<first>-<last>` (with `ES_STATE_HASH`): on each step in the
  range, print every ship's state and target, and the order governments are
  iterated in. Use this to find exactly where two runs diverge.
- `ES_MISSILE_STATS=1`: at exit, print how many projectiles of each homing
  weapon hit something, expired, or were shot down.
- `ES_SCRIPT_STATS=1`: at exit, print how often each script statement ran.
- `ES_AI_RULE_STATS=1`: at exit, print how often each AI rule ran, and how often
  it ended a ship's turn (or, for movement rules, moved the ship). Useful for
  seeing which rules a scenario exercises.

## divergence.py

`divergence.py A.log B.log` compares two runs made with `ES_STATE_DETAIL`
ship by ship: how many ships are identical, different, or in only one run over
time, and which ships diverged first. Use it to see how far a change spread.

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
- `pursuit-missiles`: replaces all four default missile guidance scripts with
  pure pursuit.
- `lead-missiles`: replaces the guidance of weapons with "leading" and
  "throttle control" with simple target leading that coasts when not lined
  up. Only those weapons can read `target-velocity` and set `thrust`, so the
  others keep their default guidance. Both are short examples of the guidance
  script language.
- `pn-missiles`: proportional navigation, which turns so that the line of
  sight to the target stops rotating. It uses `memory` to keep the line of
  sight from the step before. It is named "proportional navigation" and also
  replaces the four defaults.

## The guidance bench

`endless-sky --resources . --config tests/integration/config --guidance-bench
"Sidewinder Missile Launcher"` fires that weapon's projectiles at a target
ship flying along fixed paths (holding still ahead, fleeing, approaching,
crossing, jinking, circling, beside, behind), 20 shots each, and prints how
many hit and how quickly. The projectiles are the game's own, so the seeker
and the guidance script behave as in a battle; only the target follows a set
path. It takes a few seconds, so it is the quick way to see what an edited
script does. Options: `--bench-guidance NAME` uses another guidance script
(if the weapon has the capabilities it needs), `--bench-target MODEL` changes
the target, `--bench-shots N` the number of shots, and `--bench-replay FILE`
writes every shot's path as JSON. Don't pass `--rngseed`: each shot is seeded
separately.

For the Sidewinder Missile Launcher, against an Argosy, 20 shots each:

| Scenario | Default | Pursuit | Proportional navigation |
|---|---|---|---|
| Behind, holding still | 0 | 20 | 20 |
| Jinking | 18 | 19 | 3 |
| Crossing | 19 (137 steps) | 19 (157) | 19 (146) |
| Total of 160 | 135 | 156 | 140 |

The default never hits a target directly behind it: the built-in steering
turns by `asin(cross(facing, direction))`, which is 0 for a target exactly
behind, so the missile never turns around. Proportional navigation fixes
that, but a target that reverses every second fools it, because it steers by
how the line of sight moved since the last step.

## Keeping scripts in sync with the C++ code

Behavior that moved from C++ into scripts is checked two ways:

- **Against the old C++ code, in isolation.** `tests/unit/src/test_guidance.cpp`
  holds a copy of the homing code that `data/guidance.txt` replaced, and runs
  it and the default script for every combination of homing attributes on
  20,000 generated situations each, requiring bit-identical results. When an
  upstream change touches projectile steering, copy the new code into
  `BuiltInHoming()` and edit the scripts until the test passes.
- **Against an old build, in the whole game.** `compare-runs.sh` with
  `BINARY=` runs a scenario with a build from before the change and compares
  state hashes. With `ES_SCRIPT_STATS=1` the log also shows how often each
  script statement ran, which shows what a scenario does not cover. (The
  "leading" script and blind spots are not used by any weapon in the game
  data; the unit test covers those.) The reference build must include the
  fixes that changed runs since: the headless sprite fix
  (`ImageSet::Upload`), the cosmetic sound pitch, and choosing systems in name
  order. Otherwise runs will differ for those reasons alone.
