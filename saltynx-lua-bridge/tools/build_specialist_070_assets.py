"""Build local Switch LayeredFS assets for Specialist 0.7.1 resource correction.

No game assets are uploaded to GitHub. The complete original cache is retained;
the existing Gnawed Leaf actor/art is not replaced. New Specialist actors use
the verified Switch animation-cache format and PNG sheets are losslessly packed
as four-plane RLE PCX for the game's cache loader.
"""
from pathlib import Path
import hashlib, json, struct, xml.etree.ElementTree as ET
from PIL import Image

REPO_ROOT = Path(__file__).resolve().parents[2]
ROOT = REPO_ROOT.parents[1] if REPO_ROOT.parent.name == 'work' else REPO_ROOT
MOD = ROOT/'outputs/specialistforgooditems-switch-prep/resources/the'
SOURCE = ROOT/'outputs/specialistforgooditems-source/resources/the'
BASE = ROOT/'inputs/repentance-config'
PACKAGE = ROOT/'outputs/IsaacLuaBridge-0.7.1-resource-fix-SD'
DEST = PACKAGE/'atmosphere/contents/010021C000B6A000/romfs/rp_patch/resources'
NAMES = [
 'isaac','magdalene','cain','judas','xxx','eve','samson','azazel','lazarus','eden','lost','lazarus2','dark_judas','lilith','keeper','apollyon','forgor_bone','forgor_soul','bethany','jacob','esau',
 't_isaac','t_magdalene','t_cain','t_judas','t_bluebaby','t_eve','t_samson','t_azazel','t_lazarus','t_eden','t_lost','t_lilith','t_keeper','t_apollyon','t_forgor_bone','t_bethany','t_jacob','t_lazarus2','t_jacob2','t_forgor_soul']

def u16(n): return struct.pack('<H',n)
def u32(n): return struct.pack('<I',n)
def string(s):
    b=s.encode('ascii'); return u16(len(b))+b
def flag(s): return s.lower()=='true'
def frame(e, layer=False):
    def f(k, default=0): return float(e.get(k,str(default)))
    b=b''
    if layer: b+=struct.pack('<4f',*[f(k) for k in ('XCrop','YCrop','Width','Height')])
    b+=struct.pack('<4f',f('XPosition'),f('YPosition'),f('XScale',100)/100,f('YScale',100)/100)
    if layer: b+=struct.pack('<2f',f('XPivot'),f('YPivot'))
    b+=struct.pack('<IB',int(e.get('Delay','1')),flag(e.get('Visible','true')))
    b+=struct.pack('<8f',*[f(k,255)/255 for k in ('RedTint','GreenTint','BlueTint','AlphaTint')],*[f(k)/255 for k in ('RedOffset','GreenOffset','BlueOffset')],f('Rotation'))
    return b+struct.pack('<B',flag(e.get('Interpolated','false')))

def actor(root):
    # The native resolver treats the base as a filename, splits with getline,
    # and removes its last component. A trailing slash does not retain an empty
    # component. Match the original cache's '/.' sentinel to keep this directory.
    content=root.find('Content'); sheets=list(content.find('Spritesheets')); b=string('gfx/characters/specialist_switch/.')+u32(len(sheets))
    for s in sheets: b+=u32(int(s.get('Id')))+string(s.get('Path').replace('/','\\'))
    for tag in ('Layers','Nulls','Events'):
        entries=list(content.find(tag)); b+=u32(len(entries))
        for e in entries:
            b+=u32(int(e.get('Id')))
            if tag=='Layers': b+=u32(int(e.get('SpritesheetId')))
            b+=string(e.get('Name'))
    animations=root.find('Animations'); b+=string(animations.get('DefaultAnimation'))+u32(len(animations))
    for a in animations:
        roots=list(a.find('RootAnimation')); b+=string(a.get('Name'))+struct.pack('<IBI',int(a.get('FrameNum')),flag(a.get('Loop')),len(roots))
        for e in roots: b+=frame(e)
        for tag,idkey,is_layer in [('LayerAnimations','LayerId',True),('NullAnimations','NullId',False)]:
            entries=list(a.find(tag)) if a.find(tag) is not None else []; b+=u32(len(entries))
            for e in entries:
                b+=struct.pack('<IBI',int(e.get(idkey)),flag(e.get('Visible','true')),len(e))
                for child in e: b+=frame(child,is_layer)
        triggers=a.find('Triggers')
        if triggers is not None and len(triggers): raise ValueError('Refusing unsupported animation events')
        b+=u32(0)
    return b

def djb2(path):
    h=5381
    for c in path.replace('\\','/').lower().encode('ascii'): h=(h*33+c)&0xffffffff
    return h

def pcx_rgba(src,dst):
    image=Image.open(src).convert('RGBA'); w,h=image.size; stride=(w+1)&~1; raw=image.tobytes()
    header=bytearray(128); header[:4]=bytes([10,5,1,8]); struct.pack_into('<4H',header,4,0,0,w-1,h-1); struct.pack_into('<2H',header,12,w,h); header[65]=4; struct.pack_into('<2H',header,66,stride,1)
    out=bytearray(header)
    for y in range(h):
        for channel in range(4):
            row=raw[y*w*4+channel:(y+1)*w*4:4]+bytes(stride-w); i=0
            while i<len(row):
                n=1
                while n<63 and i+n<len(row) and row[i+n]==row[i]: n+=1
                if n>1 or row[i]>=192: out.extend([192|n,row[i]])
                else: out.append(row[i])
                i+=n
    decoded=bytearray(); i=128
    while i<len(out):
        v=out[i]; i+=1
        if v>=192: decoded.extend(bytes([out[i]])*(v&63)); i+=1
        else: decoded.append(v)
    assert len(decoded)==stride*h*4
    back=bytearray(len(raw))
    for y in range(h):
        for c in range(4): back[y*w*4+c:(y+1)*w*4:4]=decoded[(y*4+c)*stride:(y*4+c)*stride+w]
    assert back==raw, f'RGBA roundtrip mismatch: {src}'
    dst.parent.mkdir(parents=True,exist_ok=True); dst.write_bytes(out)
    return [w,h],hashlib.sha256(out).hexdigest()

def main():
    DEST.mkdir(parents=True,exist_ok=True)
    stale_music=DEST/'music.xml'
    if stale_music.exists(): stale_music.unlink()
    cache=(BASE/'animations.b').read_bytes(); old_count=struct.unpack_from('<I',cache,0)[0]
    entries=[]; seen=set()
    for index,name in enumerate(NAMES):
        src=MOD/f'specialist_{name}.anm2'; root=ET.parse(src).getroot(); payload=actor(root)
        keypath=f'resources/gfx/characters/specialist_switch/character_{index:02d}.anm2'; key=djb2(keypath)
        if key in seen or struct.pack('<I',key) in cache: raise ValueError(f'Animation-cache key collision: {keypath}')
        seen.add(key); entries.append(u32(key)+payload)
        outxml=DEST/f'gfx/characters/specialist_switch/character_{index:02d}.anm2'; outxml.parent.mkdir(parents=True,exist_ok=True); outxml.write_bytes(src.read_bytes())
    updated=bytearray(cache); struct.pack_into('<I',updated,0,old_count+len(entries)); updated.extend(b''.join(entries)); (DEST/'animations.b').write_bytes(updated)
    sheet_records={}
    for anm in MOD.glob('specialist_*.anm2'):
        root=ET.parse(anm).getroot()
        for node in root.findall('Content/Spritesheets/Spritesheet'):
            rel=node.get('Path').replace('\\','/'); src=MOD/Path(rel)
            out=DEST/'gfx/characters/specialist_switch'/Path(rel).with_suffix('.pcx')
            if rel not in sheet_records: sheet_records[rel]=pcx_rgba(src,out)
    # Keep the stock Gnawed Leaf art. Null item 4 is only the game's allocation
    # container; the runtime loads the selected Specialist ANM2 into that entry.
    (DEST/'gfx/characters/210_gnawedleaf_statue.anm2').write_bytes((BASE/'210_gnawedleaf_statue.anm2').read_bytes())
    (DEST/'gfx/characters/costumes/costume_210_gnawedleaf_statuehead.pcx').parent.mkdir(parents=True,exist_ok=True)
    (DEST/'gfx/characters/costumes/costume_210_gnawedleaf_statuehead.pcx').write_bytes((BASE/'costume_210_gnawedleaf_statuehead.pcx').read_bytes())
    costumes=ET.parse(BASE/'costumes2.xml'); item=next(c for c in costumes.getroot() if c.get('id')=='4'); item.set('priority','969'); item.set('isFlying','true'); ET.indent(costumes); costumes.write(DEST/'costumes2.xml',encoding='utf-8',xml_declaration=True)
    # Load one isolated track at runtime with Music::LoadConfig(preserve=true),
    # so the game's existing base/DLC music table is never replaced.
    music=ET.Element('music',{'root':'music/'})
    music.append(ET.Element('track',{'id':'19','name':'Specialist Switch','path':'specialist_switch.ogg','loop':'true'}))
    ET.indent(music); ET.ElementTree(music).write(DEST/'specialist_music.xml',encoding='utf-8',xml_declaration=True)
    ogg=SOURCE/'arcadespecalist.ogg'; data=ogg.read_bytes()
    if not data.startswith(b'OggS') or b'\x01vorbis' not in data[:128]: raise ValueError('Expected the mod Vorbis OGG soundtrack')
    for resource_root in (DEST,):
        out=resource_root/'music/specialist_switch.ogg'; out.parent.mkdir(parents=True,exist_ok=True); out.write_bytes(data)
    manifest={'target_cache_sha256':hashlib.sha256(cache).hexdigest(),'original_actor_records':old_count,'added_actor_records':len(entries),'added_actor_keys':[f'{djb2(f"resources/gfx/characters/specialist_switch/character_{i:02d}.anm2"):08X}' for i in range(len(entries))],'cache_original_bytes_preserved':bytes(updated[4:len(cache)])==cache[4:],'character_types':NAMES,'converted_sheets':sheet_records,'music_id':19,'music_registration':'runtime Music::LoadConfig with preserve=true; live slot 19 must be empty before registration','music_sha256':hashlib.sha256(data).hexdigest(),'music_format':'Ogg Vorbis','gnawed_leaf_art':'original restored; only priority/isFlying metadata retained for null costume 4','device_status':'requires Switch verification'}
    manifest['asset_base_path']='gfx/characters/specialist_switch/.'
    manifest['asset_base_contract']='native GetAbsolutePathFromBasePath removes the last getline component; final dot preserves the directory'
    (PACKAGE/'ASSET-BUILD.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(f'Built {len(entries)} Specialist cache actors, {len(sheet_records)} lossless PCX sheets, guarded music id 19.')

if __name__=='__main__': main()
