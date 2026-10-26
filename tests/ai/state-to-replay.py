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

"""Convert a game log with ES_STATE_DETAIL output into a compact replay file.

Usage: state-to-replay.py LOG OUTPUT.bin [--stride N]

The replay holds every ship's position, facing and target, and every
projectile's position and facing, every N steps (default 3). Projectiles are
linked from step to step by predicting where each one will be, so that a viewer
can draw trails.

File layout (little-endian):
  uint32 header length, then a JSON header padded to 4 bytes:
    {"steps": [...], "ships": [{"name", "gov"}], "governments": [...],
     "frames": F, "shipCount": S, "projectileCount": P}
  int16  ships[F][S][3]      x, y, facing in 1/100 turns (x = -32768 if absent)
  int16  targets[F][S]       index of the ship's target, or -1
  uint32 projectileStart[F+1]
  int16  projectiles[P][4]   x, y, facing in 1/100 turns, track id
  uint8  projectileFlags[P]  government index * 2 + homing
"""

import json
import math
import re
import struct
import sys

STATE = re.compile(r'^STATE (\d+) (.*?)\|(.*?) pos=(\S+),(\S+) vel=(\S+),(\S+) facing=(\S+) target=(.*)$')
PROJECTILE = re.compile(r'^PROJECTILE (\d+) lifetime=(-?\d+) homing=(\d) gov=(.*?) pos=(\S+),(\S+) vel=(\S+),(\S+) facing=(\S+)$')


def clamp16(value):
    return max(-32767, min(32767, int(round(value))))


def turns(degrees):
    return int(round((degrees % 360.) / 360. * 100.)) % 100


def main():
    log, output = sys.argv[1], sys.argv[2]
    stride = int(sys.argv[sys.argv.index('--stride') + 1]) if '--stride' in sys.argv else 3

    ship_frames = {}
    projectile_frames = {}
    for line in open(log, errors='replace'):
        if line.startswith('STATE '):
            m = STATE.match(line.rstrip('\n'))
            if m:
                step = int(m.group(1))
                ship_frames.setdefault(step, []).append(
                    (m.group(2), m.group(3), float(m.group(4)), float(m.group(5)), float(m.group(8)), m.group(9)))
        elif line.startswith('PROJECTILE '):
            m = PROJECTILE.match(line.rstrip('\n'))
            if m:
                step = int(m.group(1))
                projectile_frames.setdefault(step, []).append(
                    (int(m.group(3)), m.group(4), float(m.group(5)), float(m.group(6)),
                     float(m.group(7)), float(m.group(8)), float(m.group(9))))

    all_steps = sorted(ship_frames)
    # Link projectiles across consecutive steps: each one should appear next
    # step about one velocity further along.
    next_id = 0
    track_ids = {}
    previous = []
    for step in all_steps:
        current = projectile_frames.get(step, [])
        ids = []
        unused = {}
        for index, p in enumerate(previous):
            key = (round(p[2] + p[4]), round(p[3] + p[5]))
            unused.setdefault(key, []).append(index)
        for p in current:
            best = None
            for dx in (-2, -1, 0, 1, 2):
                for dy in (-2, -1, 0, 1, 2):
                    key = (round(p[2]) + dx, round(p[3]) + dy)
                    if key in unused and unused[key]:
                        best = unused[key].pop()
                        break
                if best is not None:
                    break
            if best is not None:
                ids.append(track_ids[(step - 1, best)])
            else:
                ids.append(next_id % 32768)
                next_id += 1
        for index, track in enumerate(ids):
            track_ids[(step, index)] = track
        previous = current

    steps = all_steps[::stride]
    ship_names = []
    ship_index = {}
    governments = []
    gov_index = {}

    def government(name):
        if name not in gov_index:
            gov_index[name] = len(governments)
            governments.append(name)
        return gov_index[name]

    ships = []
    for step in steps:
        for name, gov, *_ in ship_frames[step]:
            if name not in ship_index:
                ship_index[name] = len(ship_names)
                ship_names.append({'name': name, 'gov': government(gov)})

    frame_count = len(steps)
    ship_count = len(ship_names)
    ship_data = [-32768, 0, 0] * (frame_count * ship_count)
    target_data = [-1] * (frame_count * ship_count)
    projectile_start = [0]
    projectile_data = []
    projectile_flags = []
    for f, step in enumerate(steps):
        for name, gov, x, y, facing, target in ship_frames[step]:
            s = ship_index[name]
            base = (f * ship_count + s) * 3
            ship_data[base:base + 3] = [clamp16(x), clamp16(y), turns(facing)]
            target_data[f * ship_count + s] = ship_index.get(target, -1)
        for index, p in enumerate(projectile_frames.get(step, [])):
            homing, gov, x, y, vx, vy, facing = p
            projectile_data += [clamp16(x), clamp16(y), turns(facing), track_ids[(step, index)]]
            projectile_flags.append(government(gov) * 2 + homing)
        projectile_start.append(len(projectile_flags))

    header = json.dumps({
        'steps': steps, 'ships': ship_names, 'governments': governments,
        'frames': frame_count, 'shipCount': ship_count, 'projectileCount': len(projectile_flags),
    }, separators=(',', ':')).encode()
    header += b' ' * ((4 - len(header) % 4) % 4)
    with open(output, 'wb') as out:
        out.write(struct.pack('<I', len(header)))
        out.write(header)
        out.write(struct.pack(f'<{len(ship_data)}h', *ship_data))
        out.write(struct.pack(f'<{len(target_data)}h', *target_data))
        out.write(struct.pack(f'<{len(projectile_start)}I', *projectile_start))
        out.write(struct.pack(f'<{len(projectile_data)}h', *projectile_data))
        out.write(struct.pack(f'<{len(projectile_flags)}B', *projectile_flags))
    print(f'{output}: {frame_count} frames, {ship_count} ships, {len(projectile_flags)} projectile samples, '
          f'{next_id} projectile tracks')


if __name__ == '__main__':
    main()
