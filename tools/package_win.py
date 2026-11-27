#!/usr/bin/env python3
"""Assemble the ready-to-run Windows folder (out/win) and its manifest for tools/pc/sync.cmd.

    python3 -I tools/package_win.py --build <build dir with bo1zm.exe> --data <bo1recomp/data> --out <out/win>

Layout made (game data hard-linked from --data, so nothing is stored twice):
    bo1zm.exe, *.dll, bo1zm.toml (only if missing)
    game\      the disc's files + TU11's default.xexp / default_mp.xexp beside the executables
    update\    the rest of TU11 (patch fastfiles)
    user\0000000000000000\41560855\   the DLC packs (tools/install_dlc.py output in <data>/content)
    sync.ps1, sync.cmd (copied from tools/pc/), manifest.json
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
    ap.add_argument('--build', required=True)
    ap.add_argument('--data', required=True)
    ap.add_argument('--out', required=True)
    a = ap.parse_args()
    out = os.path.abspath(a.out)
    os.makedirs(out, exist_ok=True)

    # The port.
    for f in os.listdir(a.build):
        if f == 'bo1zm.exe' or f.lower().endswith('.dll'):
            shutil.copy2(os.path.join(a.build, f), os.path.join(out, f))
    toml = os.path.join(out, 'bo1zm.toml')
    if not os.path.exists(toml):
        shutil.copy2(os.path.join(HERE, 'pc', 'bo1zm.toml'), toml)
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
            files.append({'path': rel, 'size': st.st_size, 'sha256': c[1]})
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
