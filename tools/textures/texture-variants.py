"""Propose reusable artwork for near-identical source resolution variants.

Read originals directly from verified catalog spans. Numerical similarity is a
review aid, not permission to substitute artwork. Nothing is installed here.
"""

# Direct-command imports share the repository's grouped helper paths.
import sys as _layout_sys
from pathlib import Path as _LayoutPath
_layout_tools = _LayoutPath(__file__).resolve().parents[1]
_layout_root = _layout_tools.parent
if (_layout_tools / 'project_paths.py').is_file():
    _layout_sys.path.insert(0, str(_layout_tools))
    from project_paths import configure as _configure_paths, source_file as repo_file
    _configure_paths()
else:
    # Standalone source fixtures retain their original flat-folder behavior.
    _layout_root = _LayoutPath(__file__).resolve().parents[1]
    def repo_file(root, area, name):
        return _LayoutPath(root) / area / name

import argparse
import hashlib
import importlib.util
import io
import json
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw

spec=importlib.util.spec_from_file_location('catalog',Path(__file__).with_name('texture-catalog.py'))
catalog=importlib.util.module_from_spec(spec);spec.loader.exec_module(catalog)


def original(game,entry):
    source=entry['sources'][0]
    with (game/source['archive']).open('rb') as f:
        f.seek(source['offset']);payload=f.read(source['length'])
    if hashlib.sha256(payload).hexdigest()!=entry['sha256']:
        raise ValueError('Original source changed: '+entry['key'])
    w,h,kind=entry['width'],entry['height'],entry['archive_format'];raw=payload[128:]
    if kind in catalog.format_tool.UNCOMPRESSED:
        return catalog.format_tool.decode(kind,w,h,raw)
    fourcc={12:b'DXT1',14:b'DXT3',15:b'DXT5'}[entry['xbox_format']]
    n=((w+3)//4)*((h+3)//4)*(8 if fourcc==b'DXT1' else 16)
    return Image.open(io.BytesIO(catalog.dds_header(w,h,fourcc)+raw[:n])).convert('RGBA')


def fingerprint(image,size):
    return np.asarray(image.resize((size,size),Image.Resampling.BOX),dtype=np.int16)


def compare(a,b):
    diff=np.abs(a-b);rgb=diff[:,:,:3]
    return dict(rgb_mae=float(rgb.mean()),rgb_p95=float(np.percentile(rgb,95)),
                large_error_fraction=float(np.mean(np.max(rgb,axis=2)>24)),
                alpha_max_error=int(diff[:,:,3].max()),alpha_mae=float(diff[:,:,3].mean()))


def plausible(metrics):
    return (metrics['rgb_mae']<=6 and metrics['rgb_p95']<=16
            and metrics['large_error_fraction']<=0.015
            and metrics['alpha_max_error']<=8 and metrics['alpha_mae']<=0.5)


def build(game,root):
    entries={}
    for entry in json.loads((root/'catalog.json').read_text(encoding='utf-8'))['textures']:
        if 'key' in entry:entries.setdefault(entry['key'],entry)
    installed={p.stem:json.loads(p.read_text(encoding='utf-8')) for p in (root/'pack').glob('*.json')}
    refs={}
    for key,meta in installed.items():
        im=original(game,entries[key])
        refs[key]=dict(entry=entries[key],meta=meta,coarse=fingerprint(im,32),fine=fingerprint(im,128))
    proposals=[];checked=0
    for key,entry in entries.items():
        if key in installed:continue
        compatible=[(k,v) for k,v in refs.items()
                    if v['entry']['xbox_format']==entry['xbox_format']
                    and v['entry']['width']*entry['height']==v['entry']['height']*entry['width']
                    and v['meta']['output_size'][0]>=entry['width']
                    and v['meta']['output_size'][1]>=entry['height']]
        if not compatible:continue
        image=original(game,entry);coarse=fingerprint(image,32);checked+=1;matches=[]
        for ref,v in compatible:
            if np.mean(np.abs(coarse[:,:,:3]-v['coarse'][:,:,:3]))>6:continue
            metrics=compare(fingerprint(image,128),v['fine'])
            if plausible(metrics):matches.append((ref,metrics))
        if matches:
            ref,metrics=min(matches,key=lambda item:item[1]['rgb_mae'])
            proposals.append(dict(key=key,reference_key=ref,original_size=[entry['width'],entry['height']],
                                  reference_size=[entries[ref]['width'],entries[ref]['height']],metrics=metrics,
                                  proposed_source=installed[ref]['source'],proposed_output_size=installed[ref]['output_size'],
                                  status='requires visual UV/identity/alpha review',sources=entry['sources']))
    proposals.sort(key=lambda p:p['metrics']['rgb_mae'])
    # Small bounded sheets: source reference, candidate original, existing art.
    for page in range((len(proposals)+11)//12):
        rows=proposals[page*12:(page+1)*12];sheet=Image.new('RGB',(768,len(rows)*276),(35,35,35));draw=ImageDraw.Draw(sheet)
        for row,p in enumerate(rows):
            images=[original(game,entries[p['reference_key']]),original(game,entries[p['key']]),Image.open(p['proposed_source']).convert('RGBA')]
            labels=[p['reference_key']+' ORIGINAL',p['key']+' CANDIDATE','EXISTING ART; REVIEW ONLY']
            for col,(im,label) in enumerate(zip(images,labels)):
                im.thumbnail((256,256));sheet.paste(im,(col*256,row*276+20),im)
                draw.text((col*256+3,row*276+3),label,fill='white')
        sheet.save(root/'review'/f'variants-06-{page}.jpg',quality=90)
    return dict(schema=1,complete=False,originals_compared=checked,reference_count=len(refs),
                comparison_size=[128,128],count=len(proposals),proposals=proposals,
                limitation='Similar pixels alone do not establish shared UV layout, character identity or gameplay function. Each proposal requires visual review and final scene validation.')


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--game',type=Path,default=Path('private/game'))
    ap.add_argument('--root',type=Path,default=Path('private/texture-remaster'))
    a=ap.parse_args();report=build(a.game,a.root)
    (a.root/'variant-candidates.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({k:v for k,v in report.items() if k!='proposals'},indent=2))


if __name__=='__main__':main()
