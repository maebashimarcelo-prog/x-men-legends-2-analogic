"""Deterministic SVG -> resvg -> padded RGBA atlas -> embedded BGRA RLE."""
from pathlib import Path
import argparse, hashlib, json, subprocess
from PIL import Image

ROOT=Path(__file__).resolve().parents[1]

def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--resvg',required=True)
    args=parser.parse_args()
    version=subprocess.check_output([args.resvg,'--version'],text=True).strip()
    if version!='0.47.0': raise ValueError('Use resvg 0.47.0')
    version='resvg '+version
    entries=json.loads((ROOT/'assets/glyphs/manifest.json').read_text())['entries']
    out=ROOT/'build/modern-controller/glyphs'; out.mkdir(parents=True,exist_ok=True)
    atlas=Image.new('RGBA',(512,512),(0,0,0,0)); cells=[]
    for i,entry in enumerate(entries):
        png=out/f'{i:02}.png'
        subprocess.run([args.resvg,'--width','56','--height','56',str(ROOT/'assets/glyphs'/entry['source']),str(png)],check=True,capture_output=True)
        im=Image.open(png).convert('RGBA'); assert im.size==(56,56)
        x=(i%8)*64+4; y=(i//8)*64+4
        atlas.paste(im,(x,y))
        cells.append(dict(**entry,x=x,y=y,width=56,height=56,u0=x/512,v0=y/512,u1=(x+56)/512,v1=(y+56)/512))
    atlas.save(out/'atlas.png',compress_level=9)
    raw=atlas.tobytes('raw','BGRA'); pixels=[raw[i:i+4] for i in range(0,len(raw),4)]
    rle=bytearray(); i=0
    while i<len(pixels):
        n=1
        while n<65535 and i+n<len(pixels) and pixels[i+n]==pixels[i]: n+=1
        rle.extend(n.to_bytes(2,'little')); rle.extend(pixels[i]); i+=n
    header=['/* GENERATED from our SVGs by resvg 0.47.0. Do not edit. */',
            '#define MC_ATLAS_SIZE 512u','#define MC_GLYPH_COUNT 48u',
            'static const unsigned char mc_atlas_rle[] = {']
    header+= [','.join(str(v) for v in rle[i:i+30])+',' for i in range(0,len(rle),30)]
    header+= ['};','static const float mc_glyph_uv[48][4] = {']
    header+=['{'+','.join(f'{c[k]:.9f}f' for k in ('u0','v0','u1','v1'))+'},' for c in cells]
    header+=['};']
    (out/'controller_atlas.h').write_text('\n'.join(header)+'\n',encoding='ascii')
    info=dict(tool=version,size=[512,512],cell=64,padding=4,raw_bgra_sha256=hashlib.sha256(raw).hexdigest(),rle_bytes=len(rle),cells=cells)
    (out/'atlas.json').write_text(json.dumps(info,indent=2)+'\n',encoding='utf-8')
    # Lossless round trip and transparent gutters are required, not visual approval.
    decoded=bytearray()
    for at in range(0,len(rle),6): decoded.extend(rle[at+2:at+6]*int.from_bytes(rle[at:at+2],'little'))
    assert decoded==raw
    alpha=atlas.getchannel('A')
    for i in range(64):
        x=(i%8)*64; y=(i//8)*64
        gutters=((x,y,x+64,y+4),(x,y+60,x+64,y+64),
                 (x,y+4,x+4,y+60),(x+60,y+4,x+64,y+60))
        assert all(alpha.crop(box).getbbox() is None for box in gutters), f'Bleeding in cell {i}'
        if i>=len(entries):
            assert alpha.crop((x,y,x+64,y+64)).getbbox() is None, f'Nonempty unused cell {i}'
    print(f'PASS: {version}; 48 cells; 512x512 RGBA; padding=4; RLE={len(rle)}; SHA256={info["raw_bgra_sha256"]}')

if __name__=='__main__': main()
