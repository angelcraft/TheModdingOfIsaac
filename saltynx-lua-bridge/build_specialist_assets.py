"""Build a local LayeredFS candidate; never distributes original game assets.

The encoder must reproduce the complete original actor byte-for-byte before
replacing it. No guessed offsets or executable patches are accepted.
"""
from pathlib import Path
import copy, hashlib, json, struct, xml.etree.ElementTree as ET

def p(fmt,*v): return struct.pack('<'+fmt,*v)
def string(s):
    b=s.encode('ascii'); return p('H',len(b))+b
def flag(s): return s.lower()=='true'
def frame(e,layer=False):
    def f(k,default=0): return float(e.get(k,str(default)))
    b=b''
    if layer: b+=p('4f',*[f(k) for k in ('XCrop','YCrop','Width','Height')])
    b+=p('4f',f('XPosition'),f('YPosition'),f('XScale',100)/100,f('YScale',100)/100)
    if layer: b+=p('2f',f('XPivot'),f('YPivot'))
    b+=p('IB',int(e.get('Delay','1')),flag(e.get('Visible','true')))
    b+=p('8f',*[f(k,255)/255 for k in ('RedTint','GreenTint','BlueTint','AlphaTint')],
         *[f(k)/255 for k in ('RedOffset','GreenOffset','BlueOffset')],f('Rotation'))
    return b+p('B',flag(e.get('Interpolated','false')))

def actor(root,asset_root):
    content=root.find('Content'); sheets=list(content.find('Spritesheets'))
    b=string(asset_root)+p('I',len(sheets))
    for s in sheets: b+=p('I',int(s.get('Id')))+string(s.get('Path'))
    for tag in ('Layers','Nulls','Events'):
        entries=list(content.find(tag)); b+=p('I',len(entries))
        for e in entries:
            b+=p('I',int(e.get('Id')))
            if tag=='Layers': b+=p('I',int(e.get('SpritesheetId')))
            b+=string(e.get('Name'))
    animations=root.find('Animations'); b+=string(animations.get('DefaultAnimation'))+p('I',len(animations))
    for a in animations:
        roots=list(a.find('RootAnimation'))
        b+=string(a.get('Name'))+p('IBI',int(a.get('FrameNum')),flag(a.get('Loop')),len(roots))
        for e in roots: b+=frame(e)
        for tag,idkey,is_layer in [('LayerAnimations','LayerId',True),('NullAnimations','NullId',False)]:
            entries=list(a.find(tag)) if a.find(tag) is not None else []
            b+=p('I',len(entries))
            for e in entries:
                b+=p('IBI',int(e.get(idkey)),flag(e.get('Visible','true')),len(e))
                for child in e: b+=frame(child,is_layer)
        triggers=a.find('Triggers')
        if triggers is not None and len(triggers): raise ValueError('Event triggers unsupported')
        b+=p('I',0)
    return b

if __name__=='__main__':
    original=ET.parse('inputs/repentance-config/210_gnawedleaf_statue.anm2').getroot()
    data=Path('inputs/repentance-config/animations.b').read_bytes()
    encoded=actor(original,'gfx\\characters/.')
    found=data.find(encoded)
    print('original actor bytes',len(encoded),'exact match',found)
    if found<0:
        prefix=encoded[:200]; at=data.find(prefix)
        print('prefix match',at)
        if at>=0:
            first=next((i for i,(a,b) in enumerate(zip(encoded,data[at:])) if a!=b),len(encoded))
            print('first mismatch',first,'expected',encoded[first:first+24].hex(),'actual',data[at+first:at+first+24].hex())
        raise SystemExit('Refusing asset generation without a complete original-actor match')
    if data.find(encoded,found+1)>=0: raise SystemExit('Ambiguous actor')
    specialist=ET.parse('outputs/specialistforgooditems-switch-prep/resources/the/specialist_isaac.anm2').getroot()
    sheet=specialist.find('Content/Spritesheets/Spritesheet')
    sheet.set('Path','costumes\\costume_210_gnawedleaf_statuehead.png')
    replacement=actor(specialist,'gfx\\characters/.')
    updated=data[:found]+replacement+data[found+len(encoded):]
    # The actor hash preceding this record and every other record are preserved.
    assert updated[:found]==data[:found]
    assert updated[found+len(replacement):]==data[found+len(encoded):]
    dest=Path('outputs/IsaacLuaBridge-0.6.0-SD/atmosphere/contents/010021C000B6A000/romfs/rp_patch/resources')
    (dest/'gfx/characters/costumes').mkdir(parents=True,exist_ok=True)
    (dest/'animations.b').write_bytes(updated)
    ET.indent(specialist)
    ET.ElementTree(specialist).write(dest/'gfx/characters/210_gnawedleaf_statue.anm2',encoding='utf-8',xml_declaration=True)
    costumes=ET.parse('inputs/repentance-config/costumes2.xml')
    item=next(c for c in costumes.getroot() if c.get('id')=='4')
    assert item.get('anm2path')=='210_gnawedleaf_statue.anm2'
    item.set('priority','969');item.set('isFlying','true')
    costumes.write(dest/'costumes2.xml',encoding='utf-8',xml_declaration=True)
    from PIL import Image
    image=Image.open('outputs/specialistforgooditems-switch-prep/resources/the/sheets/isaac.png').convert('RGBA')
    width,height=image.size; raw=image.tobytes(); stride=(width+1)&~1
    header=bytearray(128);header[:4]=bytes([10,5,1,8])
    struct.pack_into('<4H',header,4,0,0,width-1,height-1)
    struct.pack_into('<2H',header,12,width,height)
    header[65]=4;struct.pack_into('<2H',header,66,stride,1)
    pcx=bytearray(header)
    for y in range(height):
        for channel in range(4):
            row=raw[y*width*4+channel:(y+1)*width*4:4]+bytes(stride-width)
            i=0
            while i<len(row):
                n=1
                while n<63 and i+n<len(row) and row[i+n]==row[i]: n+=1
                if n>1 or row[i]>=192: pcx.extend([192|n,row[i]])
                else: pcx.append(row[i])
                i+=n
    # Decode our RLE stream independently and compare all RGBA pixels.
    decoded=bytearray();i=128
    while i<len(pcx):
        value=pcx[i];i+=1
        if value>=192:
            decoded.extend(bytes([pcx[i]])*(value&63));i+=1
        else: decoded.append(value)
    assert len(decoded)==stride*height*4
    roundtrip=bytearray(len(raw))
    for y in range(height):
        for c in range(4): roundtrip[y*width*4+c:(y+1)*width*4:4]=decoded[(y*4+c)*stride:(y*4+c)*stride+width]
    assert roundtrip==raw
    (dest/'gfx/characters/costumes/costume_210_gnawedleaf_statuehead.pcx').write_bytes(pcx)
    manifest=dict(source_cache_sha256=hashlib.sha256(data).hexdigest(),
                  original_actor_offset=found,original_actor_size=len(encoded),
                  original_actor_roundtrip='byte-exact',replacement_actor_size=len(replacement),
                  other_actor_bytes='unchanged',texture_size=[width,height],texture_roundtrip='lossless RGBA',
                  native_costume_id=4,test_collectible_id=502,visual_status='unverified on device')
    Path('outputs/IsaacLuaBridge-0.6.0-SD/ASSET-BUILD.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print('Built local asset candidate; full original actor reproduced; other actors unchanged; RGBA lossless.')
