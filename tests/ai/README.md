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
- Fleets choosing which system to arrive from, ships choosing a system to
  travel to, and surveillance ships choosing a system to visit, picked from
  sets of systems ordered by memory address. They now pick from the systems
  in name order (`System::InNameOrder`).
- Ships patrolling outside a system chose their criss-crossing angle from
  their own address in memory. They now use a hash of their name. (None of
  the three scenarios reaches that code, so they never showed the problem.)

To check for this kind of problem, compare normal runs with runs that use a
different memory layout, e.g. `"a1:MALLOC_ARENA_MAX=1"` or
`"notcache:GLIBC_TUNABLES=glibc.malloc.tcache_count=0"`. All three scenarios
now give the same result under each of those.

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
- `close-escorts`: the "keep station" maneuver with two numbers changed (a
  position deadband of 50 instead of 200, and a thrust deadband of .2 instead
  of .5). In Sol traffic, the player's two escorts stay about four times
  closer to the idle flagship (mean distance 103 and 108 instead of 445 and
  431), but they never settle: their mean speed relative to the flagship goes
  from 0.17 to about 1.9, because they keep making small corrections.
- `pn-missiles`: proportional navigation, which turns so that the line of
  sight to the target stops rotating. It uses `memory` to keep the line of
  sight from the step before. It is named "proportional navigation" and also
  replaces the four defaults.
- `intercept-missiles`: a script written from scratch rather than copied
  from a default. It solves for the exact intercept course and turns by the
  full angle to it, so it also turns around for a target behind it. Like
  `lead-missiles`, it only replaces the guidance of weapons with "leading"
  and "throttle control". In the Al Dhanab scenario (seed 1) it runs about
  17,600 times.

## Editing scripts while the game runs

Start the game with `--watch-scripts` (or set `ES_WATCH_SCRIPTS=1`), and it
checks about twice a second whether any data file that defines guidance
scripts, maneuvers or AI rules has changed. When one has, it reloads the
definitions from that file between two steps of the game, and says so in the
message log:

- Only definitions that file supplied are replaced. If a plugin overrides a
  script, editing the game's own copy changes nothing; edit the plugin's.
- A script with errors is not replaced. The message says so, and the error
  log (`errors.txt`, or the terminal) gives the details.
- Projectiles already in flight switch to the new script, and the `memory`
  of a script starts over when it is reloaded.
- AI rule sets are updated in place, because several files can each supply
  some of a rule set's lists.

`live-edit.sh` checks this without anyone at the keyboard. It runs a scenario
twice with watching on and applies an edit to one run's copy of a plugin
partway through:

```sh
tests/ai/live-edit.sh build/endless-sky "AI Scenario - Sol traffic" \
    my-plugin data/maneuvers.txt 's/let position-deadband 200/let position-deadband 50/'
```

```
Reloaded "keep station", "circle around", "move to", "pick up" from maneuvers.txt.
Reloaded after step 4440.
The runs first differ at step 4500.
```

Tried this way:

- **A maneuver** (above): escorts start flying differently at once.
- **Guidance**, in Al Dhanab with seed 1: changing `asin` to `atan2` in the
  four default scripts reloaded at about step 3,750, but the runs matched
  until step 9,720, because the two only differ for a target behind the
  missile. So reloading an unchanged script changes nothing by itself.
- **AI rules**, in Al Dhanab: deleting the "aim and fire" rule mid-run. No
  projectile was in flight at any of the 136 later samples, against 86 of
  them without the edit.
- **A broken edit** (`(asin)` with no argument): the scripts weren't
  replaced, and the runs matched all the way through.

## The guidance bench

`endless-sky --resources . --config tests/integration/config --guidance-bench
"Sidewinder Missile Launcher"` fires that weapon's projectiles at a target
ship flying along fixed paths (holding still ahead, fleeing, approaching,
crossing, jinking, circling, beside, behind), 20 shots each, and prints how
many hit and how quickly. The projectiles are the game's own, so the seeker
and the guidance script behave as in a battle; only the target follows a set
path. It takes a few seconds, so it is the quick way to see what an edited
script does. Options:

- `--bench-guidance NAME` uses another guidance script (if the weapon has the
  capabilities it needs). Give it more than once to compare scripts side by
  side, one column each.
- `--bench-script FILE` loads guidance scripts from a file, with no plugin to
  install, and compares each with the weapon's own script. Each script is
  loaded under its first name only, so it doesn't replace a default. This is
  the quick way to try an edit: copy a script into a file, change it, rerun.
- `--bench-target MODEL` changes the target, and `--bench-shots N` the number
  of shots.
- `--bench-replay FILE` writes every shot's path as JSON; when comparing,
  each script gets its own file (`FILE` with the script's name added).

Don't pass `--rngseed`: each shot is seeded separately, and the same shot in
the same scenario gets the same seed whichever script steers it.

For the Sidewinder Missile Launcher, against an Argosy, 20 shots each:

| Scenario | Default | Pursuit | Proportional navigation |
|---|---|---|---|
| Behind, holding still | 0 | 20 | 20 |
| Jinking | 18 | 19 | 3 |
| Crossing | 19 (137 steps) | 19 (157) | 19 (146) |
| Total of 160 | 135 | 156 | 140 |

The default never hits a target directly behind it: the built-in steering
turns by `asin(cross(facing, direction))`, which is 0 for a target exactly
behind, so the missile never turns around. That only happens for a target
*exactly* behind, as on the bench: for one even slightly to the side, the
default turns at its full rate and comes around. With a seeker that misjudges
the bearing by 0.1 degrees (`--bench-seeker-noise 0.1`), the default hits 16
of 20 against the Hawk behind it. So the bench overstates how much the
scripts below gain by turning around; in a battle, a target is rarely
exactly behind a missile. Proportional navigation also turns around, but a
target that reverses every second fools it, because it steers by how the
line of sight moved since the last step.

### Faster targets

The Argosy is slow (5.2), so most scripts hit it. Faster, more agile targets
separate them. Total hits out of 160 for the Sidewinder Missile Launcher,
from `--bench-target`:

| Target (speed) | Default | Full turn | Intercept | No lead | Proportional navigation |
|---|---|---|---|---|---|
| Sparrow (22.9) | 74 | 94 | 97 | 58 | 78 |
| Dagger | 56 | 78 | 84 | 68 | 68 |
| Hawk (15.5) | 53 | 70 | 76 | 55 | 79 |
| Fury (13.4) | 38 | 52 | 58 | 36 | 58 |

Against these, nothing catches a target fleeing or crossing straight ahead.
Most of what "full turn" and "intercept" gain is the target exactly behind
(see above). "Intercept" (from `intercept-missiles`) gains on "full turn" in the jinking
and circling scenarios: the default's lead is one step of an estimate,
which is close enough against the Argosy (where intercept and the default
hit with the same shots) but not against a target turning quickly.
Proportional navigation does best against the circling Hawk (9 of 20, where
the default hits 1).

### One-line edits

`tests/ai/guidance-edits.txt` holds three copies of the default "leading and
throttle control" script, each with one line changed. To compare them with
the default:

```sh
endless-sky --resources . --config tests/integration/config \
    --guidance-bench "Sidewinder Missile Launcher" \
    --bench-script tests/ai/guidance-edits.txt
```

| Scenario (20 shots) | Default | Full turn | Full throttle | No lead |
|---|---|---|---|---|
| Behind, holding still | 0 | 20 (190 steps) | 0 | 0 |
| Jinking | 18 (185 steps) | 18 (185) | 19 (179) | 19 (129) |
| Crossing | 19 (137 steps) | 19 (137) | 19 (137) | 19 (158) |
| Total of 160 | 135 | 155 | 136 | 136 |

- **Full turn** computes the angle to the target with `atan2` of the cross and
  dot products instead of `asin` of the cross product alone. A target behind
  is then 180 degrees away rather than 0, so the missile turns around. Nothing
  else changes: in every other scenario its shots are identical to the
  default's. (This fixes a target *exactly* behind, which is rare in a
  battle; see above.)
- **Full throttle** never cuts thrust to turn more tightly. For this weapon it
  makes almost no difference.
- **No lead** aims at the target rather than ahead of it. That does better
  against a target that reverses every second (it isn't fooled into leading
  the wrong way) and worse against one crossing in a straight line.

### Imperfect seekers

A weapon's `homing` block can now give a `"seeker noise"` in degrees: on each
step, the seeker misjudges the bearing to its target by a random angle with
that standard deviation (drawn from the projectile's own random stream), and
the script sees the target where the seeker thinks it is. No weapon in the
game has it, so nothing changes unless a plugin adds it.
`--bench-seeker-noise DEGREES` sets it on the bench.

This is hardware, which no script can improve; what a script can choose is
how to use noisy fixes. `tests/ai/guidance-noise.txt` holds four scripts that
estimate the target's velocity themselves from successive fixes (none use
"leading"). Total hits of 160 for the Sidewinder:

| Target, noise | Pursuit | Raw lead 0.2 s | Smoothed lead 0.2 s | Smoothed lead 1 s |
|---|---|---|---|---|
| Argosy, none | 156 | 156 | 156 | 111 |
| Argosy, 3° | 156 | 93 | 153 | 94 |
| Hawk, none | 70 | 74 | 69 | 56 |
| Hawk, 1° | 74 | 63 | 73 | 50 |
| Hawk, 3° | 73 | 40 | 65 | 45 |

- Aiming 0.2 seconds ahead using the difference between the last two fixes
  is the best of these against the Hawk with a perfect seeker, and falls
  apart with 3 degrees of noise: the difference between two noisy fixes is
  mostly noise.
- Smoothing the estimate (moving it a tenth of the way toward each new
  difference) recovers most of that, but reacts more slowly.
- Aiming a whole second ahead overshoots, with or without noise.
- Pursuit barely notices the noise: the errors average out over its turns.

So how far ahead to aim, and how much to smooth, are real trade-offs that
depend on the seeker and the target, which is the kind of change a script
should be able to make. The seeker's quality is not.

## Turret scripts

`data/turrets.txt` holds scripts that move one turret, or one moving
decoration, for one step. The AI runs "idle sweep" for every turret with
nothing to aim at (and for decorations that move on their own), which sweeps
it back and forth around its idle angle at random. It reproduces
`AI::AimTurrets`' idle code exactly: the scenarios match the reference
build, with about 147,000 runs, and
`tests/unit/src/test_turretScript.cpp` compares it with a copy of the old
code on 50,000 situations, checking both the result and that it draws the
same number of random numbers. Aiming at targets is still C++; it is the
natural next step for turret scripts (point defense, for example).

## Two ways for scripts to handle lists

Choosing among several things (which flotsam to pick up, which ship to
shoot, which missile to stop) means looking at a list. There are two
versions of the same choice, which flotsam a ship chases, from
`AI::DoHarvesting`; both make exactly the choices the C++ did. Whether a
ship can pick a piece up at all stays in C++, as a game rule.

**A. Rate each candidate** (`rate "flotsam"` in `data/choices.txt`, used by
default). The game runs the script once per candidate, with the candidate's
position and velocity as inputs; the script sets a `score` or triggers
`skip`, and the game keeps the lowest score. The language needed nothing new.

**B. Choose from the list** (`choose "flotsam"` in
`tests/ai/plugins/choose-flotsam`). The script gets every candidate as a
list, loops over it with `for f in flotsam`, reads `f.position`,
`f.velocity` and `f.index`, and sets `chosen`. This needed lists of records,
a `for` statement and field access in the language. A record can only be
used through its fields: it can't be copied into a variable or memory, so
it can never outlive the run it was given to.

How they compare:

| | Rate each | Choose from the list |
|---|---|---|
| Script length | 13 lines | 12 lines, nested three levels deeper |
| Who decides "best" | the game (lowest score) | the script |
| Can weigh candidates against each other (e.g. prefer clusters, avoid what an escort is already chasing) | no | yes |
| New language features | none | lists, records, `for`, fields |
| Runs in Al Dhanab | 8,608 (once per candidate) | 487 (once per choice) |
| Time per choice, about 4.5 candidates | 7–8.5 µs | 9–10 µs |
| The same choice in C++ | 0.2–0.3 µs | |

Both give the same state hashes as the reference build in all three
scenarios, and a script that changes one threshold changes Al Dhanab.
`tests/unit/src/test_choiceScript.cpp` compares both with a copy of the old
code on 20,000 generated situations; `endless-sky-tests "[.choice-speed]"`
times them. "Rate each" is simpler and enough for "pick the best by some
measure"; "choose" is what a script needs to reason about the set as a
whole. They can coexist: the game uses a "choose" script if one is defined,
and otherwise the "rate" script.

## Language design notes

The script language is being designed as it's used. Rules so far, and why:

- **Hardware sets the limits; scripts decide within them.** Whatever an
  outfit or ship could be made "better" at (turn rate, thrust, seeker
  quality, how often a lock holds, what a sensor can see) is an attribute in
  the game data, not something a script controls. A script's outputs are
  clamped to those limits, and it learns about the world only through
  inputs and functions the host gives it. Test for any new input or
  function: could a script turn this up? If so, it belongs to the hardware.
- **Capabilities gate what a script may read or write.** `target-velocity`
  needs "leading", `thrust` needs "throttle control", and `target-facing`
  needs "optical tracking". A weapon can't use a script that needs more
  than it has; it falls back to its default.
- **Host functions are pure calculations.** `turn-toward`,
  `stopping-point` and `rendezvous-time` only compute. Anything with a side
  effect runs in C++ before the script, once, and arrives as an input: the
  "attack" maneuver's `aim` comes from `AI::TargetAim`, which also notes
  which guns are on target. Otherwise how often a script called a function
  would change the game.
- **Decision scripts choose actions by triggering effects; C++ carries
  them out.** "fight" decides between flying "attack" and only aiming the
  guns, and says which with the `attack` or `aim-guns` effect. Scripts never
  call other scripts directly, so each one stays a function from its inputs
  to its outputs and effects, which is what lets it be tested on its own.
- **A script's variables may hide inputs, but not outputs or constants.**
  When the "attack" maneuver added an `aim` input, the existing "move to"
  script, which has its own `aim`, stopped compiling, and every scenario
  changed. Letting a `let` or `memory` hide an input means the host can add
  inputs without breaking scripts already written, including plugins'.
- **A script with errors is never run.** Loading reports every error with
  its file and line, and a failed reload keeps the previous version.
- **`memory` belongs to a compiled version of a script.** Reloading an edited
  script starts every projectile's memory over, so values can't land in a
  slot that now means something else.
- **Scripts draw random numbers from whatever stream the game has made
  current.** `(random)` and `(random-int n)` take the next number exactly as
  `Random::Real()` and `Random::Int(n)` would at that point, so a script can
  reproduce C++ that draws them; "idle sweep" does. With random streams on
  (`ES_RANDOM_STREAMS=1`), that is the ship's or projectile's own stream, and
  a script that draws more or fewer numbers only changes that object. With
  them off, it shifts every later random number in the game, so experiments
  with edited scripts should turn streams on. Evaluation order is defined
  (left to right), where C++ leaves it unspecified: swapping the two draws in
  `(- (random) (random))` changes the Sol scenario, so the order the
  compiler happened to pick for the old C++ is the one the script uses.
- **Scripts can define functions, for readability.** `function name
  parameter...` with `return`. A function can only call those defined above
  it, so it can't recurse, and each has fixed variable slots (no call
  stack). It sees its parameters, its own variables, and the inputs and
  outputs, but not the script's variables, and it can't change outputs, use
  memory or effects, or stop: like the host's functions, it only computes a
  value. "keep station" uses one for "how far must the ship turn to face
  along this", which it needed twice, and the intercept example plugin uses
  them to name the intercept time and the full angle to a point. Both give
  exactly the same results as before.
- **Arithmetic works left to right, exactly as the C++ did.** `(* a b c)` is
  `(* (* a b) c)`, so scripts that copy C++ code give bit-identical results.

Open questions:

- **Sensors as functions.** Should a sensor be an input, or a function
  called only when wanted, like `(seek)`? A function only pays off when
  calling it costs something (energy, giving away position, time), and one
  reading per refresh must be the rule, or a script could call again until
  it got the answer it wanted.
- **Shared function definitions.** A script's functions belong to it, so the
  four default guidance scripts still each repeat the same lines. Outfits or
  libraries could supply definitions later.
- **Limits on computation.** Scripts already have a step limit per run. A
  smaller one for cheaper hardware would make "how much can this missile
  think" a hardware property too.

## Keeping scripts in sync with the C++ code

`upstream-watch.py check` tells you when upstream has changed C++ that a script
on this branch reproduces. `tests/ai/upstream/` holds a snapshot of each such
block (the homing code in `Projectile::Move`, `AI::Step`,
`AI::MoveEscort`, `AI::KeepStation`, `AI::CircleAround`, `AI::MoveTo`,
`AI::PickUp`, `AI::Attack`, `AI::MoveToAttack` and `AI::TurnToward`) as it
was when the scripts were written. After
`git fetch upstream`, the check compares the same blocks at `upstream/master`.
For each one that changed, it shows the diff and the commits involved, and
says which scripts to update and how to verify them:

```sh
git fetch upstream
tests/ai/upstream-watch.py check            # exits 1 if anything changed
# ... update the scripts, run the checks it names ...
tests/ai/upstream-watch.py snapshot upstream/master
```

Behavior that moved from C++ into scripts is then checked two ways:

- **Against the old C++ code, in isolation (maneuvers).**
  `tests/unit/src/test_maneuver.cpp` does the same for the maneuvers in
  `data/maneuvers.txt`, on 50,000 generated situations each: "keep station"
  (which escorts use to stay with their parent) against a copy of the old
  `AI::KeepStation`, "circle around" against `AI::CircleAround` and
  `AI::TurnToward`, "move to" against `AI::MoveTo`, and "pick up" (flying
  into flotsam) against `AI::PickUp`. "move to" and "pick up" ask the game
  where the ship would stop and how to turn it (`AI::StoppingPoint` and
  `AI::TurnToward` stay in C++), so the test gives the scripts and the copies
  the same versions of those. In Sol traffic, "keep station" runs about
  100,000 times, "move to" about 105,000 and "circle around" about 1,500;
  "move to" never uses a cruise speed there, so only the unit test covers
  that branch. "attack" (how ships close in on a target while aiming at it,
  in battle and when mining) is checked against `AI::MoveToAttack`; it runs
  about 49,000 times in Al Dhanab and 6,000 in Ne-20, and reaches every
  branch. "fight" (whether to keep a safe distance from a ship, close in on
  it, or just aim) is checked against `AI::Attack`; it runs about 76,000
  times in the scenarios, but never for a ramming ship or one backing away
  on reverse thrusters, which only the unit test covers. Situations near
  the distance where a ship backs off are generated on purpose: otherwise
  the slowdown distance, which only matters there, went untested. "pick up" runs about 3,000 times across the scenarios but
  never uses the afterburner or chases flotsam it cannot catch, which the unit
  test also covers. `AI::PickUp` also checked whether forward thrust was
  commanded before it ran; the script leaves that out, because facing
  closely enough to use the afterburner already means thrusting forward.
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
