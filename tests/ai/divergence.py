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

"""Measure how far a change spread: compare two runs ship by ship.

Usage: divergence.py LOG_A LOG_B [--every N] [--json OUT]

Both logs need ES_STATE_DETAIL output covering the same steps. For every N
steps (default 600), this prints how many ships are in exactly the same state
in both runs, how many differ, and how many exist in only one run. It then
lists the ships that diverged first. A ship "diverges" at the first step where
its position, velocity, facing or target differ.
"""

import json
import re
import sys

STATE = re.compile(r'^STATE (\d+) (.*?)\|(.*?) (pos=.*)$')


def read(path):
    steps = {}
    for line in open(path, errors='replace'):
        if not line.startswith('STATE '):
            continue
        m = STATE.match(line.rstrip('\n'))
        if not m:
            continue
        step = int(m.group(1))
        ships = steps.setdefault(step, {})
        # Ship names are not always unique; number repeated names in order.
        name = m.group(2) + '|' + m.group(3)
        key = name
        n = 1
        while key in ships:
            n += 1
            key = f'{name}#{n}'
        ships[key] = m.group(4)
    return steps


def main():
    a_path, b_path = sys.argv[1], sys.argv[2]
    every = int(sys.argv[sys.argv.index('--every') + 1]) if '--every' in sys.argv else 600
    a = read(a_path)
    b = read(b_path)
    steps = sorted(set(a) & set(b))
    if not steps:
        sys.exit('No steps in common; were both runs made with ES_STATE_DETAIL?')

    first_difference = {}
    rows = []
    for step in steps:
        ships_a, ships_b = a[step], b[step]
        same = differ = only = 0
        for key in ships_a.keys() | ships_b.keys():
            if key in ships_a and key in ships_b:
                if ships_a[key] == ships_b[key]:
                    same += 1
                    continue
                differ += 1
            else:
                only += 1
            first_difference.setdefault(key, step)
        if (step - steps[0]) % every == 0 or step == steps[-1]:
            rows.append({'step': step, 'same': same, 'differ': differ, 'only': only})

    print(f'{"step":>6} {"same":>5} {"differ":>6} {"only in one":>11}')
    for row in rows:
        print(f'{row["step"]:>6} {row["same"]:>5} {row["differ"]:>6} {row["only"]:>11}')

    ever = len(first_difference)
    total = len(set().union(*(s.keys() for s in a.values()), *(s.keys() for s in b.values())))
    print(f'\n{ever} of {total} ships differed at some point.')
    if first_difference:
        print('First to diverge:')
        for key, step in sorted(first_difference.items(), key=lambda item: item[1])[:12]:
            print(f'  step {step:>6}  {key}')

    if '--json' in sys.argv:
        with open(sys.argv[sys.argv.index('--json') + 1], 'w') as out:
            json.dump({'rows': rows, 'firstDifference': first_difference, 'ships': total}, out)


if __name__ == '__main__':
    main()
