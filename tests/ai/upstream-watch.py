#!/usr/bin/env python3
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

"""Notice when upstream changes C++ code that this branch replaced with scripts.

Usage:
  upstream-watch.py check [REF]      compare upstream code at REF (default
                                     upstream/master) with the snapshots
  upstream-watch.py snapshot REF     record the code at REF as the new snapshots

Each watched block is a piece of upstream C++ that data files on this branch
reproduce (the homing code in data/guidance.txt, the AI's turn and escort
logic in data/ai.txt, and the ship maneuvers in data/maneuvers.txt). The
snapshots in tests/ai/upstream/ hold that code as it was when the scripts were
written. When "check" reports a change, update the
scripts to match, re-run the listed checks, and then take a new snapshot.

Run "git fetch upstream" first to compare against the latest upstream code.
"""

import difflib
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SNAPSHOTS = os.path.join(HERE, 'upstream')

# name, file, regex for the block's first line, what reproduces it, how to check.
WATCHED = [
    ('projectile-homing', 'source/Projectile.cpp', r'^\s*if\(homing\)\s*$',
        'the default scripts in data/guidance.txt',
        'copy the new code into BuiltInHoming() in tests/unit/src/test_guidance.cpp and run the unit tests'),
    ('ai-step', 'source/AI.cpp', r'^void AI::Step\(',
        'the "turn" and "movement" rules in data/ai.txt (AI::TakeTurn, AIRules)',
        'compare-runs.sh against a build of upstream in the three AI scenarios'),
    ('ai-move-escort', 'source/AI.cpp', r'^void AI::MoveEscort\(',
        'the "escort" rules in data/ai.txt',
        'compare-runs.sh against a build of upstream in the three AI scenarios'),
    ('ai-keep-station', 'source/AI.cpp', r'^void AI::KeepStation\(',
        'the "keep station" maneuver in data/maneuvers.txt',
        'copy the new code into BuiltInKeepStation() in tests/unit/src/test_maneuver.cpp and run the unit tests'),
    ('ai-circle-around', 'source/AI.cpp', r'^void AI::CircleAround\(',
        'the "circle around" maneuver in data/maneuvers.txt',
        'copy the new code into BuiltInCircleAround() in tests/unit/src/test_maneuver.cpp and run the unit tests'),
    ('ai-move-to', 'source/AI.cpp', r'^bool AI::MoveTo\(',
        'the "move to" maneuver in data/maneuvers.txt',
        'copy the new code into BuiltInMoveTo() in tests/unit/src/test_maneuver.cpp and run the unit tests'),
    ('ai-turn-toward', 'source/AI.cpp', r'^double AI::TurnToward\(',
        'the turning part of the "circle around" maneuver in data/maneuvers.txt',
        'copy the new code into BuiltInTurnToward() in tests/unit/src/test_maneuver.cpp and run the unit tests'),
    ('ai-pick-up', 'source/AI.cpp', r'^void AI::PickUp\(',
        'the "pick up" maneuver in data/maneuvers.txt',
        'copy the new code into BuiltInPickUp() in tests/unit/src/test_maneuver.cpp and run the unit tests'),
    ('ai-move-to-attack', 'source/AI.cpp', r'^void AI::MoveToAttack\(',
        'the "attack" maneuver in data/maneuvers.txt',
        'copy the new code into BuiltInMoveToAttack() in tests/unit/src/test_maneuver.cpp and run the unit tests'),
]


def git(*args):
    return subprocess.run(['git', *args], cwd=HERE, check=True, capture_output=True, text=True).stdout


def extract(text, pattern):
    """Return the block starting at the line matching pattern, through its closing brace."""
    lines = text.split('\n')
    start = next((i for i, line in enumerate(lines) if re.search(pattern, line)), None)
    if start is None:
        return None
    depth = 0
    seen_open = False
    for i in range(start, len(lines)):
        # Ignore braces in comments and character literals.
        code = re.sub(r"//.*|'.'", '', lines[i])
        depth += code.count('{') - code.count('}')
        seen_open |= '{' in code
        if seen_open and depth <= 0:
            return '\n'.join(lines[start:i + 1]) + '\n'
    return None


def snapshot_path(name):
    return os.path.join(SNAPSHOTS, name + '.snapshot')


def read_snapshot(name):
    path = snapshot_path(name)
    if not os.path.exists(path):
        return None, None
    text = open(path).read()
    header, _, body = text.partition('\n')
    ref = header.removeprefix('// upstream ').strip()
    return ref, body


def code_at(ref, path):
    try:
        return git('show', f'{ref}:{path}')
    except subprocess.CalledProcessError:
        return None


def snapshot(ref):
    commit = git('rev-parse', '--short', ref).strip()
    os.makedirs(SNAPSHOTS, exist_ok=True)
    for name, path, pattern, _, _ in WATCHED:
        block = extract(code_at(ref, path) or '', pattern)
        if block is None:
            sys.exit(f'{name}: could not find /{pattern}/ in {path} at {ref}')
        with open(snapshot_path(name), 'w') as out:
            out.write(f'// upstream {commit}\n{block}')
        print(f'{name}: recorded {block.count(chr(10))} lines of {path} at {commit}')


def check(ref):
    commit = git('rev-parse', '--short', ref).strip()
    changed = 0
    for name, path, pattern, reproduced_by, how in WATCHED:
        base, old = read_snapshot(name)
        if old is None:
            print(f'{name}: no snapshot; run "upstream-watch.py snapshot <ref>" first')
            changed += 1
            continue
        new = extract(code_at(ref, path) or '', pattern)
        if new is None:
            print(f'CHANGED {name}: /{pattern}/ is no longer in {path} at {commit};'
                f' the code moved or was renamed. Find it and update {reproduced_by}.')
            changed += 1
            continue
        if new == old:
            print(f'ok      {name}: {path} is unchanged since {base}')
            continue
        changed += 1
        print(f'CHANGED {name}: {path} differs between {base} and {commit}')
        print(f'        Update {reproduced_by}, then {how}.')
        commits = git('log', '--oneline', f'{base}..{commit}', '--', f':(top){path}').strip()
        if commits:
            print('        Commits touching the file:')
            for line in commits.split('\n'):
                print('          ' + line)
        diff = difflib.unified_diff(old.split('\n'), new.split('\n'),
            f'{name} at {base}', f'{name} at {commit}', lineterm='')
        print('\n'.join('        ' + line for line in diff))
    if changed:
        print(f'\n{changed} watched block(s) need attention. After updating, run'
            f' "upstream-watch.py snapshot {ref}".')
    return 1 if changed else 0


def main():
    if len(sys.argv) < 2 or sys.argv[1] not in ('check', 'snapshot'):
        sys.exit(__doc__)
    if sys.argv[1] == 'snapshot':
        if len(sys.argv) < 3:
            sys.exit('snapshot needs a ref, e.g. upstream/master')
        snapshot(sys.argv[2])
    else:
        sys.exit(check(sys.argv[2] if len(sys.argv) > 2 else 'upstream/master'))


if __name__ == '__main__':
    main()
