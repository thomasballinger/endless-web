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

"""Look at screenshots of the simulator, for simulator-ui.sh.

Usage:
  simulator-ui.py ready IMAGE          exit 0 if the simulator is showing
  simulator-ui.py error IMAGE LINE     exit 0 if LINE of the script is marked as a mistake
  simulator-ui.py differ IMAGE IMAGE   exit 0 if the views of the battle differ
  simulator-ui.py confused IMAGE       exit 0 if a trail shows a confused seeker,
                                       and print where the newest part of one is
  simulator-ui.py glitch IMAGE         exit 0 if the editor shows that the seeker
                                       is steering instead of the script

Screenshots are of a 1600x1000 screen.
"""

import sys

from PIL import Image, ImageChops

# Where things are on a 1600x1000 screen; see SimulatorPanel::Layout().
FIRST_LINE_TOP = 50
LINE_HEIGHT = 18
GUTTER = (8, 48)
# The battle view, between the editor and the inspector.
VIEW = (680, 60, 1310, 950)
# The bar across the bottom of the editor that says why the code is glitching.
GLITCH_BAR = (8, 936, 648, 966)


def load(path):
    return Image.open(path).convert('RGB')


def ready(image):
    # The grid's background fills the bottom of the view.
    r, g, b = image.getpixel((1200, 980))
    return g > r and g > b and g > 4


def error(image, line):
    top = FIRST_LINE_TOP + (line - 1) * LINE_HEIGHT
    for y in range(top, top + LINE_HEIGHT):
        for x in range(*GUTTER):
            r, g, b = image.getpixel((x, y))
            if r > 180 and g < 130 and b < 120:
                return True
    return False


def differ(a, b):
    diff = ImageChops.difference(a.crop(VIEW), b.crop(VIEW)).convert('L')
    changed = sum(diff.histogram()[41:])
    return changed > 200


def is_confused_color(r, g, b):
    # Magenta: CONFUSED in SimulatorPanel, GLITCH in CodeEditor.
    return r > 120 and b > 120 and g < 110 and abs(r - b) < 60


def confused(image):
    # The brightest part of a confused trail is its newest.
    left, top, right, bottom = VIEW
    count, best, where = 0, 0, None
    for y in range(top, bottom):
        for x in range(left, right):
            r, g, b = image.getpixel((x, y))
            if is_confused_color(r, g, b):
                count += 1
                if r + b > best:
                    best, where = r + b, (x, y)
    if count > 20:
        print(*where)
        return True
    return False


def glitch(image):
    left, top, right, bottom = GLITCH_BAR
    count = sum(1 for y in range(top, bottom) for x in range(left, right)
                if is_confused_color(*image.getpixel((x, y))))
    return count > 30


def main(args):
    if len(args) == 2 and args[0] == 'ready':
        return ready(load(args[1]))
    if len(args) == 3 and args[0] == 'error':
        return error(load(args[1]), int(args[2]))
    if len(args) == 3 and args[0] == 'differ':
        return differ(load(args[1]), load(args[2]))
    if len(args) == 2 and args[0] == 'confused':
        return confused(load(args[1]))
    if len(args) == 2 and args[0] == 'glitch':
        return glitch(load(args[1]))
    print(__doc__)
    sys.exit(2)


if __name__ == '__main__':
    sys.exit(0 if main(sys.argv[1:]) else 1)
