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

"""Copy an integration test's saved game, adding outfits to the flagship.

Usage: add-outfits.py FILE SAVE NEW_NAME OUTFIT[:COUNT]...

Prints the "test-data" node named SAVE from FILE, renamed NEW_NAME, with the
given outfits (one of each, or COUNT) added to its first ship (the flagship).
"""

import sys


def main(args):
    if len(args) < 3:
        print(__doc__)
        return 2
    path, save, name, outfits = args[0], args[1], args[2], args[3:]
    lines = open(path).read().split('\n')
    start = lines.index('test-data "%s"' % save)
    end = start + 1
    while end < len(lines) and (not lines[end] or lines[end][0] in '\t '):
        end += 1
    block = lines[start:end]
    block[0] = 'test-data "%s"' % name
    at = block.index('\t\t\toutfits') + 1
    added = []
    for outfit in outfits:
        name, _, count = outfit.partition(':')
        added.append('\t\t\t\t"%s"' % name + (' ' + count if count else ''))
    block[at:at] = added
    print('\n'.join(block))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
