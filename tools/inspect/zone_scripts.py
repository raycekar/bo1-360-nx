#!/usr/bin/env python3
"""Extract the zlib-compressed script rawfiles (.gsc/.csc/.cfg/.menu...) from decoded BO1 zones.

    python -I tools/inspect/zone_scripts.py <out dir> <zone> [<zone> ...]

A BO1 rawfile in a zone is its name (NUL-terminated) followed shortly by a zlib stream. Later zones win
(pass base zones first, then patch zones).
"""
import os
import re
import sys
import zlib

NAME = re.compile(rb'([a-zA-Z0-9_/\\.-]{3,120}\.(?:gsc|csc|cfg|txt|menu|inc|str|csv))\x00')


def main():
    out, zones = sys.argv[1], sys.argv[2:]
    n = 0
    for z in zones:
        d = open(z, 'rb').read()
        for m in NAME.finditer(d):
            name = m.group(1).decode().replace('\\', '/')
            p = m.end()
            for k in range(p, min(p + 24, len(d) - 2)):
                if d[k] == 0x78 and ((d[k] << 8) | d[k + 1]) % 31 == 0:
                    try:
                        o = zlib.decompressobj()
                        text = o.decompress(d[k:k + 4 << 20 if False else k + (8 << 20)])
                    except zlib.error:
                        continue
                    if not o.eof or not text:
                        continue
                    dest = os.path.join(out, *name.split('/'))
                    if os.path.commonpath([os.path.abspath(dest), os.path.abspath(out)]) != os.path.abspath(out):
                        break
                    os.makedirs(os.path.dirname(dest), exist_ok=True)
                    open(dest, 'wb').write(text)
                    n += 1
                    break
    print(f'{n} files written', file=sys.stderr)


if __name__ == '__main__':
    main()
