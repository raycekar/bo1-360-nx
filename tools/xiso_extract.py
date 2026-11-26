#!/usr/bin/env python3
"""Extract an Xbox 360 game disc image (XDVDFS / XISO) to a folder.

    python -I tools/xiso_extract.py <disc.iso> <out folder> [--list]

The game partition of a 360 disc starts at one of a few fixed offsets (XGD2, XGD3, or a
trimmed image at 0). Each holds the XDVDFS volume descriptor at sector 32 of the partition.
Directories are binary trees of entries; this walks every node, not just one branch.
"""
import argparse
import os
import struct
import sys

SECTOR = 2048
MAGIC = b'MICROSOFT*XBOX*MEDIA'
PARTITION_OFFSETS = (0, 0xFD90000, 0x2080000, 0x18300000)  # trimmed, XGD2, XGD3, XGD1


def find_partition(f):
    for base in PARTITION_OFFSETS:
        f.seek(base + 32 * SECTOR)
        if f.read(len(MAGIC)) == MAGIC:
            return base
    raise SystemExit('no XDVDFS volume descriptor found')


def read_dir(f, base, sector, size):
    f.seek(base + sector * SECTOR)
    data = f.read(size)
    entries, seen, stack = [], set(), [0]
    while stack:
        off = stack.pop()
        if off in seen or off + 14 > len(data):
            continue
        seen.add(off)
        left, right, start, length, attr, nlen = struct.unpack_from('<HHIIBB', data, off)
        if left == 0xFFFF and right == 0xFFFF and start == 0xFFFFFFFF:
            continue  # padding
        name = data[off + 14:off + 14 + nlen].decode('ascii', 'replace')
        entries.append((name, start, length, attr))
        if left:
            stack.append(left * 4)
        if right:
            stack.append(right * 4)
    return entries


def walk(f, base, sector, size, rel=''):
    for name, start, length, attr in sorted(read_dir(f, base, sector, size)):
        path = f'{rel}/{name}' if rel else name
        if attr & 0x10:
            yield path, None, None
            if length:
                yield from walk(f, base, start, length, path)
        else:
            yield path, start, length


def main():
    ap = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    ap.add_argument('iso')
    ap.add_argument('out')
    ap.add_argument('--list', action='store_true', help='only list the files')
    a = ap.parse_args()
    with open(a.iso, 'rb') as f:
        base = find_partition(f)
        f.seek(base + 32 * SECTOR + len(MAGIC))
        root_sector, root_size = struct.unpack('<II', f.read(8))
        print(f'partition at 0x{base:X}, root sector {root_sector}, size {root_size}', file=sys.stderr)
        total = 0
        for path, start, length in walk(f, base, root_sector, root_size):
            dest = os.path.join(a.out, *path.split('/'))
            if start is None:
                if not a.list:
                    os.makedirs(dest, exist_ok=True)
                continue
            total += length
            if a.list:
                print(f'{length:>12}  {path}')
                continue
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            if os.path.exists(dest) and os.path.getsize(dest) == length:
                continue  # resume
            f.seek(base + start * SECTOR)
            with open(dest + '.part', 'wb') as o:
                left = length
                while left:
                    chunk = f.read(min(left, 8 << 20))
                    if not chunk:
                        raise SystemExit(f'short read in {path}')
                    o.write(chunk)
                    left -= len(chunk)
            os.replace(dest + '.part', dest)
        print(f'{total:,} bytes in files', file=sys.stderr)


if __name__ == '__main__':
    main()
