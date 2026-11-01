"""Author project-owned geometric SVGs; no fonts, downloaded art or logos."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
NAMES = ['left-left', 'left-right', 'left-up', 'left-down', 'south', 'east',
         'west', 'north', 'left-trigger', 'right-trigger', 'right-shoulder',
         'dpad-up', 'dpad-down', 'dpad-left', 'dpad-right', 'right-stick',
         'start', 'back', 'right-up', 'right-down', 'right-left', 'right-right',
         'left-shoulder', 'left-stick']
# Small original monoline lettering, expressed as geometry for deterministic art.
LETTERS = {
    'A': ['0,12 4,0 8,12', '2,7 6,7'],
    'B': ['0,12 0,0 5,0 8,2 8,4 5,6 0,6', '5,6 8,8 8,10 5,12 0,12'],
    'X': ['0,0 8,12', '8,0 0,12'], 'Y': ['0,0 4,6 8,0', '4,6 4,12'],
    'L': ['0,0 0,12 8,12'], 'R': ['0,12 0,0 5,0 8,2 8,4 5,6 0,6', '4,6 8,12'],
    'T': ['0,0 8,0', '4,0 4,12'], 'S': ['8,0 2,0 0,2 0,4 8,8 8,10 6,12 0,12'],
    '1': ['1,3 4,0 4,12', '1,12 7,12'], '2': ['0,2 2,0 6,0 8,2 8,4 0,12 8,12'],
    '3': ['0,0 8,0 4,6 8,8 8,10 6,12 0,12'],
}

def lettering(label, y=20, scale=1.6):
    width = (len(label)*11-3)*scale
    parts = [f'<g transform="translate({32-width/2:g} {y:g}) scale({scale:g})" fill="none" stroke="#fff2d3" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">']
    for i, char in enumerate(label):
        parts.extend(f'<polyline transform="translate({i*11} 0)" points="{line}"/>' for line in LETTERS[char])
    return ''.join(parts)+'</g>'

def svg(style, name):
    body = []
    face = name in ('south', 'east', 'west', 'north')
    base = '<circle cx="32" cy="32" r="27"/>' if face else '<rect x="5" y="9" width="54" height="46" rx="9"/>'
    body.append('<g fill="#152b3e" stroke="#d2b477" stroke-width="3">'+base+'</g>')
    if face:
        colors = dict(south='#84d482', east='#ec8c7e', west='#86bde8', north='#f0d377')
        body.append(f'<circle cx="32" cy="32" r="22" fill="none" stroke="{colors[name]}" stroke-width="2"/>')
        if style == 'xbox':
            body.append(lettering(dict(south='A', east='B', west='X', north='Y')[name], 20, 2))
        else:
            shapes = dict(south='<path d="M22 22L42 42M42 22L22 42"/>',
                          east='<circle cx="32" cy="32" r="13"/>',
                          west='<rect x="20" y="20" width="24" height="24"/>',
                          north='<path d="M32 18L46 43H18Z"/>')
            body.append('<g fill="none" stroke="#fff2d3" stroke-width="4" stroke-linecap="round" stroke-linejoin="round">'+shapes[name]+'</g>')
    elif name.startswith('dpad-'):
        rotation = dict(up=0,right=90,down=180,left=270)[name[5:]]
        body.append(f'<path transform="rotate({rotation} 32 32)" d="M32 17L18 32H26V46H38V32H46Z" fill="#fff2d3"/>')
    elif name in ('start', 'back'):
        if name == 'start':
            body.append('<path d="M19 22H45M19 32H45M19 42H45" stroke="#fff2d3" stroke-width="4" stroke-linecap="round"/>')
        else:
            body.append('<g fill="none" stroke="#fff2d3" stroke-width="3"><rect x="16" y="19" width="22" height="20"/><rect x="26" y="27" width="22" height="20"/></g>')
    elif name in ('left-shoulder','right-shoulder','left-trigger','right-trigger','left-stick','right-stick'):
        side = 'L' if name.startswith('left') else 'R'
        suffix = ('1' if 'shoulder' in name else '2' if 'trigger' in name else '3') if style == 'playstation' else ('B' if 'shoulder' in name else 'T' if 'trigger' in name else 'S')
        body.append(lettering(side+suffix,22,1.6))
    else:
        side, direction = name.split('-')
        rotation = dict(up=0,right=90,down=180,left=270)[direction]
        body.append(lettering(('L' if side=='left' else 'R')+('3' if style=='playstation' else 'S'),35,1.1))
        body.append(f'<path transform="rotate({rotation} 32 24)" d="M32 14L23 24H29V32H35V24H41Z" fill="#fff2d3"/>')
    return '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 64 64" width="64" height="64">\n'+''.join(body)+'\n</svg>\n'

def main():
    # This script preserves the initial sketches as an authoring reference.
    # Edited/refined SVGs are canonical inputs, never overwritten by sketches.
    for style in ('xbox','playstation'):
        for name in NAMES:
            source=ROOT/'assets/glyphs'/style/(name+'.svg')
            if source.exists() and source.read_text(encoding='utf-8')!=svg(style,name):
                raise RuntimeError('Refined SVGs are editable source. Use build-glyphs.py to regenerate the atlas; this initial sketch generator will not overwrite them.')
    entries=[]
    for style in ('xbox','playstation'):
        folder=ROOT/'assets/glyphs'/style
        folder.mkdir(parents=True,exist_ok=True)
        for index,name in enumerate(NAMES):
            (folder/(name+'.svg')).write_text(svg(style,name),encoding='utf-8',newline='\n')
            entries.append(dict(style=style,control=index,name=name,source=f'{style}/{name}.svg'))
    (ROOT/'assets/glyphs/manifest.json').write_text(json.dumps(dict(license='MIT; original project art',entries=entries),indent=2)+'\n',encoding='utf-8')
    print('PASS: 48 original geometric SVG sources; no external fonts or art')

if __name__ == '__main__': main()
