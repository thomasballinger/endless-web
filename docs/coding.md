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
    missile. The missiles do the damage they do in the game: the target's
    shields and hull are shown, and a light target can be destroyed (the
    drill then starts over); its anti-missile can shoot missiles down.
  - **Arrow keys** fly your own ship, or with **P**, the target, to dodge
    (your flying is recorded, so replays repeat it); **D** changes the drill
    (the first two are flown by the game's own AI: a pirate that comes at
    you, firing, and a ship that runs; the next three by a simple pilot: evading,
    cruising, breaking away; **1** to **9** and **0** pick one), **T** the target ship (one has
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

- **Linear scripts.** A script that `wait`s is linear: like a missilecmd
  generator, it carries on each step from where it waited, keeping its
  variables and its turn and thrust. Missiles have routines that wait until
  they're done: `turn_to(direction)` (aimed where the direction pointed when
  the turn began), `turn_by(degrees)`, `fly(steps)`, `burn(steps)`,
  `coast(steps)` and `track(steps)`. `while` and `loop` repeat lines, and
  `routine name(...)` defines your own. A plan like this commits to things,
  so it's simple to write and easy to dodge:

      if target
          turn_to(target.direction)
      fly(45)
      track(60)

  The editor shows "waiting N steps" on the line a missile is waiting on.
  New linear firmware: Dead Reckoning (Kor Mereti and Kor Sestor automata),
  Pursuit Bursts (the Free Worlds, ported from missilecmd), Dogleg (bounty
  hunters) and Hunter-Seeker (Militia: sweep, close in, strike). Each hits
  less often than reactive guidance, against nimble targets especially.
- **A smarter missile.** Lock Keeper (Syndicate) keeps a smoothed estimate
  of its target, solves for the intercept with a loop, and keeps flying at
  the estimate when jamming blinds its seeker: it beats the factory programs
  on every launcher in the bench, most of all against jammers. It's in
  `data/programs.txt`, as are the rest.
- **Escort roles.** In the workshop, give each escort a role, or one role to
  all of them (**R** cycles it). A role is a list of gambits, the first that
  applies deciding what the escort does:

      if hull < .3
          retreat
      elif threat && threat.distance < 1200
          attack_threat
      else
          station = vec(180, -150)    # right of the leader, a bit behind
          keep_station

  It can keep station anywhere around you, attack the nearest threat, your
  target or its own, retreat, or leave it to the usual AI (`independent`).
  Bodyguard, Striker and Picket come with the game; the exchange sells
  Shieldwall (Navy: a wall between you and the threat), Wolf Pack (pirates:
  pile onto whatever's hurt) and Hit and Run (Syndicate, linear: strike for
  ten seconds, fall back for five). See `data/roles.txt`.
- **Trying roles against a raider.** Click an escort's role in the workshop
  to open it in the escort workshop: the escort plays the role while a
  raider attacks: a pirate in the same kind of ship as the escort, flown by
  the game's own AI (it goes after you or the escort, as it would in the
  game). The fight is the game's: ships fire as the AI decides (you fire
  only if your "Automatic firing" preference is on), shots fly and hit, and
  damage takes shields, then hull, so the bars (shields above, hull below)
  fall for real. A raider is beaten once it is disabled (another comes from
  elsewhere), and "retreat when hurt" gambits get tried. When the role says
  attack, the game's AI flies the escort in, as it would in the game; when
  the role chooses nothing, the usual AI flies it. Every edit replays the
  fight; **Ctrl+S** gives the role to the escort (or all of them).
- **Editing roles in flight.** Select an escort (click it in the escort list,
  bottom left) and open the scripts pane (**K**): its role runs there with
  its values. **EDIT** pauses the game; change the code, and once you stop
  typing the escort flies the new version (marked lines if it doesn't
  compile). **Escape** flies on.

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
  one: daemons have no editor or simulator of their own. In the simulators,
  every ship (yours, the target, the escort, the raider) moves exactly as
  ships move in the game, with the game's own code (drag only while
  thrusting, so a ship that stops thrusting coasts; energy and heat count),
  and fights as in the game: missiles and shots hit and do the game's
  damage, anti-missile shoots missiles down, and ships are disabled and
  destroyed (the drill starts over soon after). Carriers don't launch their
  fighters, though, and there are no asteroids.
- A role's "threat" is simply the nearest hostile ship to the leader; there
  is no list of all of them, or of incoming missiles, to choose from yet.
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
