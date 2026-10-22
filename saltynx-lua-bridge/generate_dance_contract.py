"""Generate exact-build fingerprints and verify native call-through identities."""
import hashlib,json,struct,sys
from pathlib import Path
from nro_plt import resolve_plt_imports

data=Path(sys.argv[1]).read_bytes(); parsed=resolve_plt_imports(data)
windows=[(0x291BBC,0x44),(0x2F2170,0x40),(0x3C0CC4,0x28),
         (0x290C38,0x8A8),(0x2914E0,0x16C),(0x277B74,0xB0)]
calls=[(0x6730A0,0x3C0CC4),(0x6716F0,0x290C38),(0x678230,0x2914E0)]
for plt,_ in calls: windows.append((plt,16))
mod=struct.unpack_from('<I',data,4)[0]; dyn=mod+struct.unpack_from('<i',data,mod+4)[0]
tags={}
for p in range(dyn,dyn+0x10000,16):
    tag,value=struct.unpack_from('<qQ',data,p)
    if not tag: break
    tags[tag]=value
if tags.get(36): raise ValueError('RELR unsupported')
relocs=[]
for at,size in [(7,8),(23,2)]:
    for p in range(tags[at],tags[at]+tags[size],24):
        target,info,addend=struct.unpack_from('<QQq',data,p)
        if any(o<=target<o+n for o,n in windows): raise ValueError('Relocated code range')
        relocs.append((target,info,addend))
vtable=[r for r in relocs if r[0]==0xAACF38]
assert len(vtable)==1
symbol_index=vtable[0][1]>>32
symbol=struct.unpack_from('<IBBHQQ',data,tags[6]+24*symbol_index)
assert symbol[4]==0xA37100, 'Player constructor vtable identity mismatch'
entries=[]
for offset,size in windows:
    chunk=data[offset:offset+size]; h=0xcbf29ce484222325
    for b in chunk: h=((h^b)*0x100000001b3)&0xffffffffffffffff
    entries.append(dict(offset=offset,size=size,fnv64=h,sha256=hashlib.sha256(chunk).hexdigest()))
bindings=[]
for plt,target in calls:
    entry=parsed.entries[plt]
    sym=struct.unpack_from('<IBBHQQ',data,tags[6]+24*entry.symbol_index)
    assert sym[4]==target, 'PLT prototype evidence mismatch'
    bindings.append(dict(got=entry.got_address,target=target,symbol=entry.name))
dest=Path(sys.argv[2]);dest.parent.mkdir(parents=True,exist_ok=True)
dest.write_text('#pragma once\n// Exact-build drift guards; no original executable bytes.\nstatic const struct { uintptr_t offset; size_t size; u64 fingerprint; } kDanceGuards[] = {\n'+''.join('    {0x%X,0x%X,0x%016XULL},\n'%(e['offset'],e['size'],e['fnv64']) for e in entries)+'};\nstatic const struct { uintptr_t got, target; } kDanceBindings[] = {\n'+''.join('    {0x%X,0x%X},\n'%(e['got'],e['target']) for e in bindings)+'};\n')
dest.with_suffix('.json').write_text(json.dumps(dict(input_sha256=parsed.sha256,windows=entries,bindings=bindings),indent=2)+'\n')
print('Native costume call flow, constructor vtable and drift guards verified.')
