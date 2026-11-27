"""Rebuild the BO1 zone Salsa20 key: run the game's auth-header key builder (TU11 default.xex sub_821D80A0) in a tiny
PowerPC interpreter on your own decrypted image. usage: t5_zone_key.py <default.image.bin> <any .ff> [mode=0]
Mode 0 is the key the zones use. No key is stored in this repository."""
import sys,struct
img=bytearray(open(sys.argv[1],'rb').read()); ff=sys.argv[2]; mode=int(sys.argv[3]) if len(sys.argv)>3 else 0
FN,MEMCPY,EXIT=0x821D80A0,0x824BD060,0x825EA984
B=0x82000000; M=0xffffffff
mem={}  # sparse extra memory
def rd(a,n):
    out=bytearray()
    for i in range(n):
        x=(a+i)&M
        out.append(mem[x] if x in mem else (img[x-B] if 0<=x-B<len(img) else 0))
    return bytes(out)
def wr(a,b):
    for i,v in enumerate(b): mem[(a+i)&M]=v
HDR=0x70080000; wr(HDR,open(ff,'rb').read()[0x10:0x10+300])
r=[0]*32; r[1]=0x70070000; r[3]=HDR; r[4]=mode
cr=[(0,0,0)]*8; CTR=0; CA=0  # each: (lt,gt,eq)
def s16(v): return v-0x10000 if v&0x8000 else v
def setcr(f,a,b,signed=True):
    if signed: a=a-(1<<32) if a&0x80000000 else a; b=b-(1<<32) if b&0x80000000 else b
    cr[f]=(a<b,a>b,a==b)
pc=0x821D80A8
for step in range(100000):
    if pc==EXIT: break
    w=struct.unpack('>I',rd(pc,4))[0]; op=w>>26; rt=(w>>21)&31; ra=(w>>16)&31; rb=(w>>11)&31; imm=w&0xffff; npc=pc+4
    if op==14: r[rt]=((r[ra] if ra else 0)+s16(imm))&M
    elif op==15: r[rt]=((r[ra] if ra else 0)+(imm<<16))&M
    elif op==24: r[ra]=r[rt]|imm
    elif op==25: r[ra]=r[rt]|(imm<<16)
    elif op==7: r[rt]=(r[ra]*s16(imm))&M
    elif op==32: r[rt]=struct.unpack('>I',rd(r[ra]+s16(imm),4))[0]
    elif op==34: r[rt]=rd(r[ra]+s16(imm),1)[0]
    elif op==36: wr(r[ra]+s16(imm),struct.pack('>I',r[rt]))
    elif op==37: ea=(r[ra]+s16(imm))&M; wr(ea,struct.pack('>I',r[rt])); r[ra]=ea
    elif op==38: wr(r[ra]+s16(imm),bytes([r[rt]&0xff]))
    elif op==11: setcr(rt>>2,r[ra],s16(imm)&M)
    elif op==10: setcr(rt>>2,r[ra],imm,False)
    elif op==21:  # rlwinm
        sh=rb; mb=(w>>6)&31; me=(w>>1)&31
        rot=((r[rt]<<sh)|(r[rt]>>(32-sh)))&M if sh else r[rt]
        m=0
        for i in range(32):
            if (mb<=me and mb<=i<=me) or (mb>me and (i>=mb or i<=me)): m|=1<<(31-i)
        r[ra]=rot&m
    elif op==18:
        li=w&0x3fffffc; li=li-0x4000000 if li&0x2000000 else li; t=(pc+li)&M
        if w&1:
            if t==MEMCPY: wr(r[3],rd(r[4],r[5])); 
            else: raise SystemExit(f'call {t:08X} at {pc:08X}')
        else: npc=t
    elif op==16:
        bo=rt; bi=ra; bd=w&0xfffc; bd=bd-0x10000 if bd&0x8000 else bd
        f,b=bi>>2,bi&3; bit=cr[f][b] if b<3 else False
        if not bo&4:
            CTR=(CTR-1)&M; ctr_ok=(CTR!=0) if not bo&2 else (CTR==0)
        else: ctr_ok=True
        cond_ok=True if bo&0x10 else (bit if bo&8 else not bit)
        take=ctr_ok and cond_ok
        if take: npc=(pc+bd)&M
    elif op==31:
        xo=(w>>1)&0x3ff
        if xo==444: r[ra]=r[rt]|r[rb]
        elif xo==40:
            r[rt]=(r[rb]-r[ra])&M
            if w&1: setcr(0,r[rt],0)
        elif xo==23: r[rt]=struct.unpack('>I',rd(r[ra]+r[rb],4))[0]
        elif xo==151: wr(r[ra]+r[rb],struct.pack('>I',r[rt]))
        elif xo==0: setcr(rt>>2,r[ra],r[rb])
        elif xo==32: setcr(rt>>2,r[ra],r[rb],False)
        elif xo==824:  # srawi
            v=r[rt]-(1<<32) if r[rt]&0x80000000 else r[rt]; ca=1 if v<0 and (v&((1<<rb)-1)) else 0
            r[ra]=(v>>rb)&M; CA=ca
        elif xo==202: r[rt]=(r[ra]+CA)&M
        elif xo==467: CTR=r[rt]
        elif xo==266: r[rt]=(r[ra]+r[rb])&M
        else: raise SystemExit(f'op31 xo {xo} at {pc:08X}')
    else: raise SystemExit(f'op {op} at {pc:08X}')
    pc=npc
base=(0x82840000-0x6018)
print(rd(base+0x1a68,32).hex())
