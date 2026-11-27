#!/usr/bin/env python3
r"""Assemble the ready-to-run Windows folder (out/win) and its manifest for tools/pc/sync.cmd.

    python3 -I tools/package_win.py --build <build dir> [--build <build dir> ...] --data <bo1recomp/data> --out <out/win>
    ... --queue 02-qos-fix --notes "what to test"   -> a test build in <out>/builds/02-qos-fix/ (see below)

Layout made (game data hard-linked from --data, so nothing is stored twice):
    bo1zm.exe / bo1mp.exe, *.dll (from the first --build), bo1zm.toml / bo1mp.toml (only if missing)
    game\      the disc's files + TU11's default.xexp / default_mp.xexp beside the executables
    update\    the rest of TU11 (patch fastfiles)
    user\0000000000000000\41560855\   the DLC packs (tools/install_dlc.py output in <data>/content)
    sync.ps1, sync.cmd (copied from tools/pc/), manifest.json
--queue NAME puts only the port (exe, DLLs, toml, TEST.txt) in <out>/builds/NAME/ and leaves the top-level build as it
is. The exe finds game\, update\ and user\ two levels up (common/bo1_paths.h), so a sync copies only the new ~60 MB
folder, never the game data again. <out>/BUILDS.txt lists the queue. The .map/.pdb go to <out>/../maps/NAME/ for
tools/crash_triage.py. To drop an old test build: delete its folder here and package again (the sync removes it).
manifest.json lists every file but sync.cmd with its size and SHA-256 (cached in <out>/../.win_hashcache.json by
size + mtime, so a rebuild only hashes what changed). The folder holds game data: it never leaves your machines.
"""
import argparse
import hashlib
import json
import os
import shutil
import time

HERE = os.path.dirname(os.path.abspath(__file__))
SKIP_MANIFEST = {'sync.cmd', 'manifest.json'}
KEEP_LOCAL = {'bo1zm.toml', 'bo1mp.toml'}  # user settings (any folder): copied once, never overwritten by a sync


def link(src, dst):
    os.makedirs(os.path.dirname(dst), exist_ok=True)
    if os.path.exists(dst):
        if os.path.samefile(src, dst):
            return
        os.remove(dst)
    try:
        os.link(src, dst)
    except OSError:
        shutil.copy2(src, dst)


def link_tree(src_root, dst_root, skip=()):
    for dp, dn, fn in os.walk(src_root):
        for f in fn:
            if f in skip:
                continue
            s = os.path.join(dp, f)
            link(s, os.path.join(dst_root, os.path.relpath(s, src_root)))


def sha256(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(8 << 20), b''):
            h.update(chunk)
    return h.hexdigest()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--build', required=True, action='append')
    ap.add_argument('--data', required=True)
    ap.add_argument('--out', required=True)
    ap.add_argument('--queue', help='name of a test build folder under <out>/builds (for example 02-qos-fix)')
    ap.add_argument('--notes', default='', help='what changed and what to test (goes to TEST.txt and BUILDS.txt)')
    a = ap.parse_args()
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)
    port = os.path.join(out, 'builds', a.queue) if a.queue else out
    os.makedirs(port, exist_ok=True)

    # The port.
    # The DLLs come from the first build: every build compiles the same SDK with the same flags.
    for i, build in enumerate(a.build):
        for f in os.listdir(build):
            if f in ('bo1zm.exe', 'bo1mp.exe') or (i == 0 and f.lower().endswith('.dll')):
                shutil.copy2(os.path.join(build, f), os.path.join(port, f))
                if f.endswith('.exe'):
                    toml = os.path.join(port, f[:-4] + '.toml')
                    if not os.path.exists(toml):
                        shutil.copy2(os.path.join(HERE, 'pc', f[:-4] + '.toml'), toml)
    if a.queue:
        maps = os.path.join(os.path.dirname(out), 'maps', a.queue)
        os.makedirs(maps, exist_ok=True)
        for build in a.build:
            for f in os.listdir(build):
                if f.endswith(('.map', '.pdb')):
                    shutil.copy2(os.path.join(build, f), os.path.join(maps, f))
        with open(os.path.join(port, 'TEST.txt'), 'w', newline='\r\n') as f:
            f.write(f'Test build {a.queue} ({time.strftime("%Y-%m-%d %H:%M")})\n\n{a.notes}\n\n'
                    'Start bo1zm.exe (or bo1mp.exe) in this folder. It uses game, update and user from the top folder.\n'
                    'The log is in this folder. Send the log if something goes wrong.\n')
    builds_dir = os.path.join(out, 'builds')
    if os.path.isdir(builds_dir):
        lines = ['Test builds, oldest first. Each folder runs on its own; game data is shared from this folder.', '']
        for name in sorted(os.listdir(builds_dir)):
            t = os.path.join(builds_dir, name, 'TEST.txt')
            if os.path.isfile(t):
                note = open(t).read().split('\n\n', 2)
                lines.append(f'builds\\{name}: ' + (note[1].strip().splitlines() or [''])[0])
        with open(os.path.join(out, 'BUILDS.txt'), 'w', newline='\r\n') as f:
            f.write('\n'.join(lines) + '\n')
    for f in ('sync.ps1', 'sync.cmd'):
        shutil.copy2(os.path.join(HERE, 'pc', f), os.path.join(out, f))

    # Game data.
    link_tree(os.path.join(a.data, 'game'), os.path.join(out, 'game'))
    upd = os.path.join(a.data, 'update')
    for x in ('default.xexp', 'default_mp.xexp'):
        if os.path.exists(os.path.join(upd, x)):
            link(os.path.join(upd, x), os.path.join(out, 'game', x))
    link_tree(upd, os.path.join(out, 'update'), skip={'default.xexp', 'default_mp.xexp'})
    content = os.path.join(a.data, 'content')
    if os.path.isdir(content):
        link_tree(content, os.path.join(out, 'user'))

    # Manifest.
    cache_path = os.path.join(os.path.dirname(out), '.win_hashcache.json')
    cache = json.load(open(cache_path)) if os.path.exists(cache_path) else {}
    files = []
    for dp, dn, fn in os.walk(out):
        dn[:] = [d for d in dn if d not in ('.sync',)]
        for f in sorted(fn):
            p = os.path.join(dp, f)
            rel = os.path.relpath(p, out).replace(os.sep, '/')
            if rel in SKIP_MANIFEST or rel.startswith('user/cache/') or rel.endswith('.part'):
                continue
            st = os.stat(p)
            key = f'{st.st_size}:{st.st_mtime_ns}'
            c = cache.get(rel)
            if not c or c[0] != key:
                c = [key, sha256(p)]
                cache[rel] = c
            e = {'path': rel, 'size': st.st_size, 'sha256': c[1]}
            if os.path.basename(rel) in KEEP_LOCAL:
                e['keep'] = True
            files.append(e)
    files.sort(key=lambda e: e['path'])
    json.dump(cache, open(cache_path, 'w'))
    manifest = {'version': 1, 'built': time.strftime('%Y-%m-%d %H:%M:%S'), 'files': files}
    with open(os.path.join(out, 'manifest.json.part'), 'w') as f:
        json.dump(manifest, f, indent=0)
    os.replace(os.path.join(out, 'manifest.json.part'), os.path.join(out, 'manifest.json'))
    total = sum(e['size'] for e in files)
    print(f'{len(files)} files, {total / 2**30:.2f} GB -> {out}')


if __name__ == '__main__':
    main()
