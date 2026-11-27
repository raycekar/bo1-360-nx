#!/usr/bin/env python3
"""Lay out an xwin cache's unpacked MSVC CRT + Windows SDK packages for clang on Linux.

xwin 0.10.0's own `splat` step crashes here (segfault); its `unpack` output is fine. This does the splat:

    python3 tools/xwin_splat.py [--msix=<msiextract root>] [--dl=<xwin cache>/dl] <xwin cache>/unpack|- <out> [<extra source dirs to scan for #includes> ...]

Layout (what cmake/windows-xwin.cmake expects):
    crt/include, crt/lib/x86_64, sdk/include/{ucrt,um,shared,winrt,cppwinrt}, sdk/lib/{um,ucrt}/x86_64
Linux is case-sensitive and Windows code is not: every #include spelling found in the headers (and in the extra
source dirs) that differs from the file on disk gets a symlink, and every library gets a lower-case link.
"""
import os
import re
import shutil
import sys

INC = re.compile(rb'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.M)
PRAGMA_LIB = re.compile(rb'#\s*pragma\s+comment\s*\(\s*lib\s*,\s*"([^"]+)"', re.I)


def merge(src, dst):
    if not os.path.isdir(src):
        return 0
    n = 0
    for dp, dn, fn in os.walk(src):
        rel = os.path.relpath(dp, src)
        os.makedirs(os.path.join(dst, rel), exist_ok=True)
        for f in fn:
            t = os.path.join(dst, rel, f)
            if not os.path.exists(t):
                shutil.copy2(os.path.join(dp, f), t)
                n += 1
    return n


def main():
    global MSIX
    args = sys.argv[1:]
    MSIX = None
    DL = None
    while args and args[0].startswith('--'):
        k, v = args.pop(0).split('=', 1)
        if k == '--msix':
            MSIX = v
        elif k == '--dl':
            DL = v
    unpack, out = args[0], args[1]
    extra = args[2:]
    pk = {d: os.path.join(unpack, d) for d in os.listdir(unpack)} if unpack != '-' else {}
    n = 0
    for name, p in pk.items():
        if '.CRT.Headers.' in name:
            n += merge(os.path.join(p, 'include'), os.path.join(out, 'crt', 'include'))
        elif '.CRT.x64.Desktop.' in name or '.CRT.x64.Store.' in name:
            n += merge(os.path.join(p, 'lib', 'x64'), os.path.join(out, 'crt', 'lib', 'x86_64'))
        elif name.startswith('Win11SDK') or name.startswith('Win10SDK') or name == 'ucrt.msi':
            inc = os.path.join(p, 'include')
            if os.path.isdir(inc):
                for sub in os.listdir(inc):
                    n += merge(os.path.join(inc, sub), os.path.join(out, 'sdk', 'include', sub))
            for kind in ('um', 'ucrt'):
                n += merge(os.path.join(p, 'lib', kind, 'x64'), os.path.join(out, 'sdk', 'lib', kind, 'x86_64'))
    # CRT straight from the .vsix zips (xwin's unpack of them can be partial): include/** and lib/x64/*.lib.
    if DL and os.path.isdir(DL):
        import zipfile
        for f in sorted(os.listdir(DL)):
            if not (f.endswith('.vsix') and '.CRT.' in f):
                continue
            z = zipfile.ZipFile(os.path.join(DL, f))
            for info in z.infolist():
                parts = info.filename.split('/')
                if len(parts) < 7 or parts[:4] != ['Contents', 'VC', 'Tools', 'MSVC'] or info.is_dir():
                    continue
                rest = parts[5:]
                if rest[0] == 'include':
                    dest = os.path.join(out, 'crt', 'include', *rest[1:])
                elif rest[:2] == ['lib', 'x64'] and len(rest) == 3 and rest[2].lower().endswith('.lib'):
                    dest = os.path.join(out, 'crt', 'lib', 'x86_64', rest[2])
                else:
                    continue
                if not os.path.exists(dest):
                    os.makedirs(os.path.dirname(dest), exist_ok=True)
                    with z.open(info) as src, open(dest, 'wb') as o:
                        shutil.copyfileobj(src, o)
                    n += 1
    # msiextract output (xwin did not unpack some MSIs): <root>/<pkg>/Program Files/Windows Kits/10/{Lib,Include}/<ver>/..
    if MSIX and os.path.isdir(MSIX):
        for pkgname in os.listdir(MSIX):
            kits = os.path.join(MSIX, pkgname, 'Program Files', 'Windows Kits', '10')
            incroot = os.path.join(kits, 'Include')
            for ver in sorted(os.listdir(incroot)) if os.path.isdir(incroot) else []:
                for sub in os.listdir(os.path.join(incroot, ver)):
                    n += merge(os.path.join(incroot, ver, sub), os.path.join(out, 'sdk', 'include', sub))
            for ver in sorted(os.listdir(os.path.join(kits, 'Lib'))) if os.path.isdir(os.path.join(kits, 'Lib')) else []:
                for kind in ('um', 'ucrt'):
                    n += merge(os.path.join(kits, 'Lib', ver, kind, 'x64'), os.path.join(out, 'sdk', 'lib', kind, 'x86_64'))
    print(f'{n} files copied')

    # Case links. Index every header dir by lower-case name.
    inc_dirs = [os.path.join(out, 'crt', 'include')] + [
        os.path.join(out, 'sdk', 'include', d) for d in ('ucrt', 'um', 'shared', 'winrt', 'cppwinrt')]
    inc_dirs = [d for d in inc_dirs if os.path.isdir(d)]
    index = {}  # lower relpath -> (dir, real relpath)
    for d in inc_dirs:
        for dp, dn, fn in os.walk(d):
            for f in fn:
                rel = os.path.relpath(os.path.join(dp, f), d).replace(os.sep, '/')
                index.setdefault(rel.lower(), (d, rel))
    wanted = set()
    scan = inc_dirs + extra
    libs = set()
    for d in scan:
        for dp, dn, fn in os.walk(d):
            dn[:] = [x for x in dn if x != '.git']
            for f in fn:
                if not f.lower().endswith(('.h', '.hpp', '.hxx', '.inl', '.c', '.cc', '.cpp', '.cxx', '.ipp', '')):
                    continue
                try:
                    data = open(os.path.join(dp, f), 'rb').read()
                except OSError:
                    continue
                for m in INC.finditer(data):
                    wanted.add(m.group(1).decode('latin-1').replace('\\', '/'))
                for m in PRAGMA_LIB.finditer(data):
                    libs.add(m.group(1).decode('latin-1'))
    links = 0
    for w in wanted:
        hit = index.get(w.lower())
        if not hit:
            continue
        d, real = hit
        path = os.path.join(d, w)
        if os.path.lexists(path):
            continue
        os.makedirs(os.path.dirname(path), exist_ok=True)
        os.symlink(os.path.relpath(os.path.join(d, real), os.path.dirname(path)), path)
        links += 1
    # Libraries: lower-case links, plus the spellings #pragma comment(lib) uses.
    for d in (os.path.join(out, 'crt', 'lib', 'x86_64'), os.path.join(out, 'sdk', 'lib', 'um', 'x86_64'),
              os.path.join(out, 'sdk', 'lib', 'ucrt', 'x86_64')):
        if not os.path.isdir(d):
            continue
        files = {f.lower(): f for f in os.listdir(d)}
        for low, real in list(files.items()):
            for name in {low} | {l for l in libs if l.lower() == low} | ({l + '.lib' for l in libs if (l + '.lib').lower() == low}):
                p = os.path.join(d, name)
                if not os.path.lexists(p):
                    os.symlink(real, p)
                    links += 1
    print(f'{links} case links')


if __name__ == '__main__':
    main()
