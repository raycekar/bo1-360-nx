#!/usr/bin/env python3
"""Which kernel/XAM imports of a decrypted XEX the SDK implements, stubs, or lacks.
    python -I tools/inspect/import_audit.py <xexdump summary.json>   (run from the repository root)
"""
import json,re,os,sys
SDK='sdk'
summ=json.load(open(sys.argv[1]))['imports']
tables={}
for lib,path in (('xam','src/kernel/xam/export_table.inc'),('xboxkrnl','src/kernel/xboxkrnl/export_table.inc')):
    t={}
    for m in re.finditer(r'XE_EXPORT\(\s*\w+\s*,\s*(0x[0-9A-Fa-f]+)\s*,\s*(\w+)\s*,\s*(\w+)',open(os.path.join(SDK,path)).read()):
        t[int(m.group(1),16)]=(m.group(2),m.group(3))
    tables[lib]=t
impl={};
for dp,dn,fn in os.walk(os.path.join(SDK,'src')):
    for f in fn:
        if not f.endswith(('.cpp','.h','.inc')): continue
        s=open(os.path.join(dp,f),errors='replace').read()
        for m in re.finditer(r'REX_EXPORT(_STUB_RETURN|_STUB|)\s*\(\s*__imp__(\w+)',s):
            kind={'':'impl','_STUB':'stub','_STUB_RETURN':'stub_return'}[m.group(1)]
            impl.setdefault(m.group(2),(kind,os.path.relpath(os.path.join(dp,f),SDK)))
        for m in re.finditer(r'REX_(?:HOOK|HOOK_RAW)\s*\(\s*__imp__(\w+)',s):
            impl.setdefault(m.group(1),('impl',os.path.relpath(os.path.join(dp,f),SDK)))
from collections import Counter
c=Counter(); rows=[]
for lib in summ:
    L=lib['library']; t=tables.get(L,{})
    for o in lib['ordinals']:
        name,kind=t.get(o,(f'ordinal_{o}','?'))
        st,where=impl.get(name,('MISSING' if kind=='kFunction' else kind,''))
        c[(L,st)]+=1; rows.append((L,o,name,kind,st,where))
for k,v in sorted(c.items()): print(k,v)
print()
for r in rows:
    if r[4] not in ('impl',): print(*r)
