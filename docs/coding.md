# Coding in Endless Sky

In this fork, every ship's flight computer and every guided missile runs
scripts, and the player can watch them, read them, interfere with them, and
write their own. This page is a guide for playtesting it, and a list of
ideas to try next. (`tests/ai/README.md` has the technical details.)

## Trying it

Build as usual, then start a new pilot (or load one). Things to look for:

- **The HUD's code maps.** Under your target's name, and beside your ship's
  status (on the right), a small map of the code each is running: one dash
  per line of code, lit when that line just ran. With nothing to show, it
  says why: which built-in AI rule the ship is following ("turn: wait in
  transit"), "idle", "no code scanner", "out of scanner range"...
- **Reading code.** Click a map, or press **K** ("Show scripts"), to open a
  pane of the code beside the HUD, with what each line worked out. Buttons
  at the top switch between your ship, your target, your selected escorts,
  and any missile homing in on you; tabs switch between the scripts each
  ran (a ship may run a maneuver, a turret sweep, and its missiles'
  guidance).
- **Outfits** (Systems category):
  - **Code Scanner**, 12,000 credits, sold even at New Boston: reads other
    ships' code within 500 (more scanners reach farther, like other
    scanners). Your own ships' code never needs one.
  - **Code Jammer** and **Code Injector**, sold wherever the Common Outfits
    are: the jammer garbles your code for other scanners; the injector
    crashes a scanned ship's scripts for a few seconds (it drifts, without
    firing) or spins a missile away. With the pane open, **X** (or HACK) arms
    it: within five seconds, click a line of the code that ran on this step
    (a lit one) to inject there; a line that didn't run misses, and still
    costs the injector its energy and recharge. Without the pane, X hacks the
    target outright.
    It costs 200 energy and needs ten seconds to recharge.
  - **Cognition Accelerator**, also with the Common Outfits: while the code
    pane is open, time runs at a quarter speed (the edges of the screen turn
    violet), long enough to read an incoming missile's code and hack it. It
    drains energy while it runs, and stops when the ship runs low.
- **Factions' code looks like theirs.** The Navy's is stamped "cleared for
  fleet use", the Syndicate's threatens your warranty, pirates' code was
  "ripped out of a dead merchant's flight computer", and alien code (Hai,
  Korath, Quarg, Wanderer, Coalition, Pug) is in their own script: you can
  see it run, and its numbers, but not read it.
- **Factions fly their own way.** Pirates jink as they attack, Korath raiders
  close in burning hard, and merchant freighters lumber along at three
  quarters of their top speed. These are new scripts, not the original AI;
  Preferences > Settings > "Classic AI scripts" puts the original back.
- **The missile workshop.** Landed anywhere with a spaceport or outfitter,
  press **W** ("Workshop"). It lists your fleet's guided weapons (Meteor
  Missile Launchers are the cheapest; they're sold with the Common
  Outfits). Choose one to open it in the simulator:
  - **E** edits the program; once you pause typing, the battle replays
    from the first moment the change made a difference, so you see what it
    does at once.
  - Programs are C-like: `turn = turn_toward(target.intercept)`,
    `if target.bearing / turn * 1.5 > target.steps_away`. What the seeker
    sees of its target (`target.position`, `target.direction`...) can only
    be used inside `if target`. `data/guidance.txt` lists every input and
    function.
  - **Ctrl+S** installs it on all your ships' weapons of that kind,
    **Ctrl+D** goes back to the factory's. Your programs are saved with your
    game.
  - The editor follows one missile from launch to impact, then the next;
    **[** and **]** (or clicking) follow another. Beside the range, a
    close-up shows its thrust, drag, turn, life, lock and the odds of
    keeping it. Hover over a vector's value to see it drawn from the
    missile.
  - **Arrow keys** fly your own ship (your flying is recorded, so replays
    repeat it); **D** changes the drill (the first three are flown by a
    pilot: evading, cruising, breaking away), **T** the target ship (one has
    radar jammers that confuse seekers), **Space** pauses, **,** and **.**
    step back and forward, and dragging on the timeline goes to any step.
  - The weapon's hardware limits its program: only "leading" seekers can
    read `target.velocity` or `target.intercept`, only "throttle control"
    can set `thrust`.

Things worth trying: make your Meteors lead their targets (they can't, on
basic hardware: the workshop will tell you why), make them coast instead of
overshooting, or make a missile that gives up and flies straight when it
loses its target. Hack a pirate mid-dogfight. Fly near Hai space with a
scanner.

## Known issues

- Nothing in the test scenarios fires a homing missile at the player, so the
  "Incoming missile" button has only been checked by reading the code.
- The Al Dhanab test scenario isn't quite deterministic on a busy machine:
  its last few state hashes sometimes differ between two runs of the same
  build (the reference build too).
- Balance is guesswork: scanner range, jammer strength, hack length and
  recharge, outfit prices.

## Ideas to try next

Things that would build on what's here, roughly from smallest to largest:

- **Translators.** An outfit (or a mission reward) that decodes one alien
  faction's code: the Hai translator, bought in Hai space.
- **A scorecard in the workshop.** The guidance bench already scores
  scripts against every drill; show its table next to the simulator, so
  improving a program is a game of its own.
- **Code as loot.** Boarding a disabled ship copies its scripts into your
  library ("salvaged pirate attack routine"), to install on your escorts, or
  to study.
- **Programming escorts.** The same workshop for your escorts' maneuvers
  ("keep station", "attack"), with a fleet drill in the simulator.
- **Programmable turrets.** Point-defense priorities and idle sweeps.
- **Firmware for sale.** Outfits that are programs: "Southbound Missile
  Firmware 2.0" for sale, licensed, with a header comment to match.
- **Hardware for thinking.** Missile computers with a limit on how many steps
  a program may take, so better hardware runs smarter code.
- **Decoys that fool code.** A confused seeker could report a phantom target
  instead of taking over, so programs can learn to distrust a target that
  jumps (this changes how missiles behave, so it was left out so far).
- **Counter-hacking.** NPCs with injectors (Korath hackers?) going after your
  missiles and ships, and a firewall outfit.
- **Law and order.** Flying with hacked or unlicensed code in Republic space
  is illegal, like illegal outfits; the Syndicate puts bounties on leaked
  code.
- **Missions.** Debug a stranded freighter's autopilot; steal a prototype
  guidance program; win a missile-programming contest on a Navy base.
- **Live debugging in flight.** Slow time and step through a missile's
  program while it chases its target.
- **Different editors for different factions.** A block-based editor for some
  factions' hardware, text for others. Guidance programs are mostly a tree of
  tests on the inputs ending in a turn and a thrust, so an alien faction
  could see them as a decision tree, with the branch that ran lit up; another
  faction's code could stay in prefix form (which scripts can still use).
- **Linear scripts.** A faction whose programs run once, start to finish, over
  a missile's flight ("burn for 2 seconds, then turn toward the target until
  it is close, then..."), rather than once per step.
- **Code modules for sale.** The ready-made inputs and functions
  (`target.intercept`, `turn_toward`) could be things a missile's hardware or
  firmware provides, bought separately.
