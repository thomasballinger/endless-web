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

- **The escort workshop.** In the workshop, **E** (or the last row) opens how
  your escorts keep station near your ship: the "keep station" maneuver.
  Your ship flies a drill (cruising, zigzagging, sprinting then stopping,
  holding still; **D** changes it, and the arrow keys fly it yourself) and
  an escort keeps station on it with the program being edited; **T** tries
  another escort ship. Every edit replays the run with the new program,
  the old escort's course showing faintly, and the inspector says how far
  off station it is, now and on average. **Ctrl+S** installs it for your
  escorts (no outfit needed, for now; you don't even need an escort to try
  it).

- **Factions' missiles fly their own firmware.** Pirate missiles corkscrew
  on the way in (harder for anti-missile turrets), Navy and Militia ones use
  proportional navigation, Korath ones come in at an angle from a side they
  pick at random, Hai ones coast until they face you, merchants' turn
  gently. Each is a "program" in `data/programs.txt`; "Classic AI scripts"
  turns them off too. Watch an incoming missile's code (with a scanner) to
  see which one it runs.
- **The program exchange and your library.** In the workshop, **X** (or the
  tab) opens the exchange: each faction's programs, sold where its authors
  live (Navy programs in Republic space, the Syndicate's in Syndicate
  space...), or all of them to a pilot who chose the **Missile Lab** start.
  Number keys buy. Back on the fleet tab (**F**), each weapon, your
  escorts and your defense daemon have a program chooser: the arrows (or
  **Shift+number**, **Shift+E**, **H**) cycle through the factory's
  program, the ones you bought that its hardware can run, and the ones in
  your library. Installing an edited program in the simulator puts it in
  your library (named for the weapon, unless you renamed it on its first
  line), so you can keep several and switch between them.
- **Defense Daemon** (Systems, with the Common Outfits, 120,000): with a
  Code Scanner and a Code Injector, it hacks incoming missiles for you.
  Whenever the injector is ready, its program rates every missile chasing
  you or your escorts that the scanner can read, and the injector hacks the
  highest scorer. The factory program ("nearest first") takes whichever will
  hit first; the exchange sells "Triage" (Syndicate: most damage soonest,
  ignoring missiles that lost their lock), "Last Instant" (Korath: waits
  until a missile is too close to recover) and "Swarm Breaker" (Hai: the
  farthest missile that will run out of fuel before it recovers). **Korath
  and Hai ships run their own daemons against your missiles.** In the
  simulator, **H** has the target run each daemon in turn: hacked missiles
  get a lightning bolt on their trail and "HACKED" in their log.
- **Code as loot.** In the code pane, **COPY** (beside HACK) copies the
  guidance or maneuver you're reading into your library as "... (copied)",
  as long as jamming doesn't garble it. Copy a pirate missile's corkscrew as
  it closes in, then choose it for your own launcher, or open it in the
  simulator and improve it.

Things worth trying: make your Meteors lead their targets (they can't, on
basic hardware: the workshop will tell you why), make them coast instead of
overshooting, or make a missile that gives up and flies straight when it
loses its target. Hack a pirate mid-dogfight. Fly near Hai space with a
scanner.

## Known issues

- Nothing in the test scenarios fires a homing missile at the player, so the
  "Incoming missile" button has only been checked by reading the code.
  (`tests/ai/daemon-test.sh` adds pirate Ravens to Al Dhanab to test the
  Defense Daemon, and could be used for that.)
- You can choose a defense daemon, buy one, or copy one, but not yet write
  one: daemons have no editor or simulator of their own.
- A copied maneuver (say, a pirate's "fight") shows up as a choice for how
  your escorts keep station, which is probably not what it's for.
- Faction firmware is only tuned roughly against the guidance bench; some
  of it (Flanker, Patient Strike) hits less often than the factory's on the
  bench's drills, which is fine for flavor but may not be for balance.
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
- **A daemon workshop.** The missile simulator with the editor on the
  target's defense daemon instead of the missiles' guidance: write a daemon,
  and watch which missiles it picks.
- **Boarding loot.** Boarding a disabled ship copies all its scripts, even
  jammed ones ("salvaged pirate attack routine").
- **More escort maneuvers.** "attack", "fight" and "move to" in the escort
  workshop, with a fleet drill.
- **Programmable turrets.** Point-defense priorities and idle sweeps.
- **Hardware for thinking.** Missile computers with a limit on how many steps
  a program may take, so better hardware runs smarter code.
- **Decoys that fool code.** A confused seeker could report a phantom target
  instead of taking over, so programs can learn to distrust a target that
  jumps (this changes how missiles behave, so it was left out so far).
- **Counter-hacking.** NPCs with injectors going after your ships (Korath
  daemons already go after your missiles), and a firewall outfit; missiles
  whose guidance notices it was hacked.
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
