"""Build a local, resumable 2x pack from verified original game textures.

Uses an explicitly supplied Real-ESRGAN ncnn executable. Never downloads models,
changes settings, publishes assets, or replaces an existing pack. Color is
upscaled at 4x and reduced to 2x; alpha and specialist data use original samples.
"""
import argparse
from contextlib import contextmanager
import hashlib
import importlib.util
import io
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import time

import numpy as np
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[1]
BUDGET = 6144 * 1024 * 1024
KEY = re.compile(r"[0-9a-f]{16}")
# Appearance classifications do not prove semantics. Uncertain/specialist
# textures are faithfully resized rather than hallucinated by a color model.
AI_CATEGORIES = {'environment', 'prop', 'weapon', 'character', 'foliage',
                 'environment_alpha', 'environment_sky'}


def module(name, filename):
    spec = importlib.util.spec_from_file_location(name, HERE / filename)
    result = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(result)
    return result


variants = module('upscale_variants', 'texture-variants.py')
compiler = module('upscale_compiler', 'texture-pack.py')
audit = module('upscale_audit', 'texture-worklist.py')


def sha(data):
    return hashlib.sha256(data).hexdigest()


def write_json(path, value):
    tmp = path.with_suffix(path.suffix + '.tmp')
    tmp.write_text(json.dumps(value, indent=2) + '\n', encoding='utf-8')
    tmp.replace(path)


def contained(root, relative):
    target = (root / relative).resolve()
    if not target.is_relative_to(root.resolve()):
        raise ValueError('Original source escapes game directory')
    return target


def original(game, entry):
    for source in entry['sources']:
        contained(game, source['archive'])
    result = variants.original(game, entry)
    if result.size != (entry['width'], entry['height']):
        raise ValueError('Decoded original dimensions differ from catalog')
    return result


def dimensions(entry):
    w, h = entry['width'], entry['height']
    if any(n <= 0 or n > 1024 or n & (n-1) for n in (w, h)):
        raise ValueError('Expected original powers of two up to 1024')
    return w*2, h*2


def choose_route(entry, classification):
    # Lookup/mask channels must retain their texels, not inferred color detail.
    if entry['xbox_format'] in (0, 25, 26):
        return 'original-channel-resample'
    width,height=entry.get('width',64),entry.get('height',64)
    small=min(width,height);ratio=max(width,height)/small
    category=classification.get('visual_category')
    if category in AI_CATEGORIES:
        if small<32 or (small<=32 and ratio>=4):
            return 'original-thin-resample'
        return 'realesrgan-x4plus'
    description=classification.get('description','').lower()
    if max(width,height)<=32 or any(word in description for word in ('noise','scanline','lookup','dither')):
        return 'original-texel-resample'
    return 'original-specialist-resample'


def texel_route(route):
    return route in ('original-channel-resample','original-thin-resample','original-texel-resample')


def finalize(source, enhanced, size, route='realesrgan-x4plus'):
    nearest=texel_route(route)
    rgb = enhanced.convert('RGB').resize(size, Image.Resampling.NEAREST if nearest else Image.Resampling.LANCZOS)
    rgba = rgb.convert('RGBA')
    rgba.putalpha(source.getchannel('A').resize(size, Image.Resampling.NEAREST if nearest else Image.Resampling.BILINEAR))
    return rgba


def storage(root):
    files = []
    for child in root.iterdir():
        if child.is_dir() and child.name not in ('authored', 'pack'):
            files.extend(p for p in child.rglob('*') if p.is_file())
    return audit.temporary_storage(root, files)


@contextmanager
def scratch(root):
    import tempfile
    parent = root / 'preview'
    parent.mkdir(exist_ok=True)
    folder = Path(tempfile.mkdtemp(prefix='original-upscale-', dir=parent)).resolve()
    if folder.parent != parent.resolve():
        raise ValueError('Scratch directory escaped intended preview parent')
    try:
        (folder/'in').mkdir(); (folder/'out').mkdir()
        yield folder
    finally:
        # Only remove our own flat PNG files after checking resolved parents.
        for name in ('in', 'out'):
            sub = folder/name
            for p in sub.iterdir():
                if p.resolve().parent != sub.resolve() or not p.is_file() or p.suffix != '.png':
                    raise ValueError('Unexpected scratch file; preserved for inspection')
                p.unlink()
            sub.rmdir()
        folder.rmdir()


def build_plan(root, game, policy_path, overrides=None):
    catalog_data = (root/'catalog.json').read_bytes()
    catalog = json.loads(catalog_data)
    policy = json.loads(policy_path.read_text(encoding='utf-8'))
    retained = set(policy['keys'])
    approved_icons = policy.get('approved_upscaled_character_icon_keys', [])
    if (not isinstance(approved_icons, list)
            or any(not isinstance(k, str) or not KEY.fullmatch(k) for k in approved_icons)
            or len(approved_icons) != len(set(approved_icons))):
        raise ValueError('Invalid approved character-icon keys')
    approved_icons = set(approved_icons)
    if approved_icons & retained:
        raise ValueError('Character icon cannot be both retained and approved for upscaling')
    classes = {r['key']: r for r in json.loads(
        (root/'visual-classification.json').read_text(encoding='utf-8'))['records']}
    entries = {}
    for e in catalog['textures']:
        if 'key' in e:
            if not KEY.fullmatch(e['key']):
                raise ValueError('Invalid texture key')
            entries.setdefault(e['key'], e)
    overrides=overrides or {}
    if approved_icons - entries.keys():
        raise ValueError('Unknown approved character-icon key')
    if any(entries[k]['width'] != 64 or entries[k]['height'] != 64
           or entries[k]['xbox_format'] in (0, 25, 26) for k in approved_icons):
        raise ValueError('Approved character icons must be 64x64 color textures')
    if set(overrides)-entries.keys():raise ValueError('Unknown review-override key')
    rows = []
    for key, e in sorted(entries.items()):
        if key in retained:
            continue
        size = dimensions(e)
        route = choose_route(e, classes.get(key, {}))
        if key in approved_icons:
            route = 'realesrgan-x4plus'
        if key in overrides:
            if overrides[key]['route'] not in ('original-review-fallback','original-texel-resample'):
                raise ValueError('Unsupported review override')
            route=overrides[key]['route']
        # Lossless output for original alpha/data; opaque color uses BC1.
        fmt = 'DXT1' if e['xbox_format'] == 12 and not texel_route(route) else 'RGBA8'
        size_bytes, _ = audit.dds_size(*size, 8 if fmt == 'DXT1' else None)
        maximum_bytes, _ = audit.dds_size(*size, None)
        rows.append(dict(key=key, output_size=list(size), route=route,
                         expected_dds_bytes=size_bytes, maximum_dds_bytes=maximum_bytes, original_payload_sha256=e['sha256'],
                         alpha_resampling='NEAREST' if texel_route(route) else 'BILINEAR',
                         review_override=overrides.get(key),
                         category=classes.get(key, {}).get('visual_category', 'unclassified'),
                         description=classes.get(key, {}).get('description', ''),
                         occurrences=len(e['sources'])))
    total = sum(r['maximum_dds_bytes'] for r in rows)
    if total + len(rows)*8192 > BUDGET:
        raise ValueError('Projected pack exceeds 6 GiB including metadata allowance')
    return dict(schema=1, catalog_sha256=sha(catalog_data), scale=2,
                supported_keys=len(entries), retained_original_keys=sorted(retained & entries.keys()),
                unsupported_payloads=sum('key' not in e for e in catalog['textures']),
                candidate_count=len(rows), expected_dds_bytes=sum(r['expected_dds_bytes'] for r in rows), maximum_dds_bytes=total,
                routes={name:sum(r['route']==name for r in rows) for name in sorted({r['route'] for r in rows})},
                limitation='Offline candidate coverage is not scene validation; specialist assets use faithful resampling.',
                records=rows), entries


def model_identity(exe, model_dir):
    paths = [exe, model_dir/'realesrgan-x4plus.param', model_dir/'realesrgan-x4plus.bin']
    return {p.name: sha(p.read_bytes()) for p in paths}


def compile_one(source, enhanced, row, entry, output, identities, elapsed, used_bytes=None):
    key = row['key']; dest = output/f'{key}.dds'
    if dest.exists() or dest.with_suffix('.json').exists():
        raise ValueError('Refusing to overwrite candidate ' + key)
    size = tuple(row['output_size'])
    result = finalize(source, enhanced, size, row['route'])
    # DXT1 is used only when opacity is actually constant. Everything with
    # alpha/data remains RGBA8, retaining the selected original alpha samples exactly.
    fmt = 'DXT1' if entry['xbox_format'] == 12 and not texel_route(row['route']) and result.getchannel('A').getextrema()==(255,255) else 'RGBA8'
    data, mips = compiler.encode(result, fmt)
    decoded = Image.open(io.BytesIO(data)).convert('RGBA')
    alpha_error = int(np.max(np.abs(np.asarray(decoded.getchannel('A'),dtype=np.int16)-np.asarray(result.getchannel('A'),dtype=np.int16))))
    if alpha_error:
        raise ValueError('Encoded original alpha changed')
    if row['route']=='realesrgan-x4plus':
        a=np.asarray(source.convert('RGB'),dtype=np.float32)
        b=np.asarray(decoded.convert('RGB').resize(source.size,Image.Resampling.BOX),dtype=np.float32)
        weight=np.asarray(source.getchannel('A'),dtype=np.float32)/255
        visible=weight>0.125
        blank=bool(np.any(visible) and np.max(a[visible])>32 and np.max(b[visible])<2)
        error=float((np.mean(np.abs(a-b),axis=2)*weight).sum()/max(1,float(weight.sum())))
        if blank or error>40:
            raise ValueError('Neural output failed fidelity guard; inspect original/compiled pixels before retrying '+key)
    meta = dict(key=key, source='verified-original-game-payload',
                source_size=list(source.size), output_size=list(size), resampled=True,
                channels='alpha' if entry['xbox_format']==25 else ('luminance' if entry['xbox_format']==0 else 'color'),
                format=fmt, mips=mips, bytes=len(data), sha256=sha(data),
                original_payload_sha256=entry['sha256'], decoded_original_rgba_sha256=sha(source.tobytes()),
                method=row['route'], model_files=identities if row['route']=='realesrgan-x4plus' else {},
                ai_output_size=list(enhanced.size) if row['route']=='realesrgan-x4plus' else None,
                final_color_resampling='NEAREST' if texel_route(row['route']) else 'LANCZOS',
                original_alpha_resampling='NEAREST' if texel_route(row['route']) else 'BILINEAR',
                decoded_alpha_max_error=alpha_error, seconds=elapsed,
                validation='offline format/alpha checks passed; scene validation pending')
    metadata = (json.dumps(meta,indent=2)+'\n').encode('utf-8')
    used = used_bytes if used_bytes is not None else sum(p.stat().st_size for p in output.iterdir() if p.is_file())
    if used+len(data)+len(metadata)>BUDGET or len(data)>24*1024*1024:
        raise ValueError('Pack/per-file budget exceeded')
    with dest.open('xb') as f: f.write(data)
    with dest.with_suffix('.json').open('xb') as f: f.write(metadata)
    audit.check_asset(dest, meta, entry)
    return decoded


def comparison(root, source, result, key):
    # Selected durable review only; full inference PNGs are discarded.
    sheet=Image.new('RGB',(1024,550),(28,28,28)); draw=ImageDraw.Draw(sheet)
    for x,im,title in [(0,source,'Original, enlarged for comparison'),(512,result,'2x original-based candidate')]:
        im=im.convert('RGB'); im.thumbnail((504,504),Image.Resampling.NEAREST)
        # Upsize small images for readable comparison without changing aspect.
        factor=min(504/im.width,504/im.height)
        im=im.resize((round(im.width*factor),round(im.height*factor)),Image.Resampling.NEAREST)
        sheet.paste(im,(x,40)); draw.text((x+4,5),title,fill='white')
    draw.text((4,530),key+' - scene validation pending',fill='white')
    sheet.save(root/'review'/f'original-upscale-{key}.jpg',quality=94)


def infer(args, input_path, output_path):
    command=[str(args.exe.resolve()),'-i',str(input_path),'-o',str(output_path),
             '-m',str(args.models.resolve()),'-n','realesrgan-x4plus','-s','4',
             '-t','128','-j','1:2:2','-g',str(args.gpu)]
    env=dict(os.environ,OMP_NUM_THREADS='4',OMP_WAIT_POLICY='PASSIVE')
    process=subprocess.run(command,stdout=subprocess.PIPE,stderr=subprocess.PIPE,timeout=90,env=env)
    if process.returncode:
        raise ValueError('Real-ESRGAN failed: '+process.stderr.decode(errors='replace')[-2000:])


def large_image(args, source, temp):
    # Overlap prevents independent tile borders from becoming visible. Only
    # central pixels are copied; the complete 4x image lives in RAM.
    result=Image.new('RGB',(source.width*4,source.height*4))
    for y in range(0,source.height,256):
        for x in range(0,source.width,256):
            box=(max(0,x-24),max(0,y-24),min(source.width,x+280),min(source.height,y+280))
            tile=source.crop(box).convert('RGB'); inp=temp/'in/tile.png'; out=temp/'out/tile.png'
            tile.save(inp); infer(args,inp,out)
            with Image.open(out) as image:
                if image.size!=(tile.width*4,tile.height*4): raise ValueError('Unexpected tile dimensions')
                w=min(256,source.width-x);h=min(256,source.height-y)
                result.paste(image.crop(((x-box[0])*4,(y-box[1])*4,(x-box[0]+w)*4,(y-box[1]+h)*4)),(x*4,y*4))
            inp.unlink();out.unlink()
    return result


def _run(args):
    root=args.root.resolve(); game=args.game.resolve(); output=args.output.resolve()
    if not output.is_relative_to(root/'authored') and output != root/'pack':
        raise ValueError('Candidates must be under authored/ or the active pack')
    if output == root/'pack':
        raise ValueError('Build candidates under authored/; activate separately after auditing')
    if (output.parent/'activation.json').exists():
        raise ValueError('This pack is already activated; use a new candidate directory for a new production task')
    overrides=json.loads(args.review_overrides.read_text(encoding='utf-8')) if getattr(args,'review_overrides',None) else {}
    plan, entries = build_plan(root, game, args.policy, overrides)
    variants.catalog.format_tool.verify_mapping(game/'default.xbe')
    output.mkdir(parents=True,exist_ok=True)
    provenance=output.parent
    write_json(provenance/'plan.json',plan)
    if args.plan_only:
        print(json.dumps({k:v for k,v in plan.items() if k!='records'},indent=2)); return
    identities=model_identity(args.exe,args.models)
    selected=set(args.keys or [])
    if selected-set(entries): raise ValueError('Unknown requested key')
    if selected & set(plan['retained_original_keys']): raise ValueError('Requested key is retained-original')
    rows=[r for r in plan['records'] if not selected or r['key'] in selected]
    if args.limit is not None:
        if args.limit < 1: raise ValueError('Limit must be positive')
        rows=sorted(rows,key=lambda r:(-r['occurrences'],r['key']))[:args.limit]
    signature=dict(catalog_sha256=plan['catalog_sha256'],scale=2,model_files=identities)
    config=provenance/'run-config.json'
    if config.exists() and json.loads(config.read_text())!=signature:
        raise ValueError('Catalog/model changed: use a new candidate output directory')
    write_json(config,signature)
    pending=[]
    for row in rows:
        dds=output/f"{row['key']}.dds"; meta=dds.with_suffix('.json')
        if dds.exists() or meta.exists():
            if not dds.exists() or not meta.exists(): raise ValueError('Incomplete candidate pair; inspect '+str(dds))
            m=json.loads(meta.read_text()); audit.check_asset(dds,m,entries[row['key']])
            if (m['original_payload_sha256']!=row['original_payload_sha256'] or m['method']!=row['route']
                    or m['output_size']!=row['output_size'] or m.get('original_alpha_resampling')!=row['alpha_resampling']):
                raise ValueError('Candidate provenance differs from plan')
        else:pending.append(row)
    started=time.monotonic(); done=len(rows)-len(pending)
    used_bytes=sum(p.stat().st_size for p in output.iterdir() if p.is_file())
    pending.sort(key=lambda r:(r['route']!='realesrgan-x4plus',-r['occurrences'],r['key']))
    cursor=0; last_storage_check=-64
    while cursor < len(pending):
        if cursor-last_storage_check >= 64:
            state=storage(root); headroom=state['temporary_headroom_bytes'];last_storage_check=cursor
        if headroom < 8*1024*1024:
            raise ValueError('Need at least 8 MiB temporary headroom before inference')
        batch=[]; reserved=2*1024*1024
        route=pending[cursor]['route']
        while cursor < len(pending) and len(batch)<16:
            row=pending[cursor]; entry=entries[row['key']]
            if row['route']!=route: break
            need=entry['width']*entry['height']*55+65536 if route=='realesrgan-x4plus' else 0
            if batch and reserved+need>headroom: break
            batch.append(row);cursor+=1;reserved+=need
            if reserved>headroom: break  # A large image uses bounded overlapping tiles.
        if shutil.disk_usage(root).free < sum(r['maximum_dds_bytes'] for r in batch)+128*1024*1024:
            raise ValueError('Insufficient free disk space')
        originals={r['key']:original(game,entries[r['key']]) for r in batch}
        t=time.monotonic()
        with scratch(root) as temp:
            neural=route=='realesrgan-x4plus'
            large=neural and reserved>headroom
            if neural and not large:
                for key,source in originals.items(): source.convert('RGB').save(temp/'in'/f'{key}.png')
                infer(args,temp/'in',temp/'out')
            for row in batch:
                key=row['key']; source=originals[key]
                if large:
                    enhanced=large_image(args,source,temp)
                elif neural:
                    with Image.open(temp/'out'/f'{key}.png') as image: enhanced=image.convert('RGB')
                else: enhanced=source
                if neural and enhanced.size != (source.width*4,source.height*4):
                    raise ValueError('Unexpected neural output dimensions')
                result=compile_one(source,enhanced,row,entries[key],output,identities,(time.monotonic()-t)/len(batch),used_bytes)
                used_bytes += (output/f'{key}.dds').stat().st_size+(output/f'{key}.json').stat().st_size
                if key in set(args.compare or []): comparison(root,source,result,key)
                done+=1
                if done%16==0 or done==len(rows):
                    print(json.dumps(dict(done=done,total=len(rows),key=key,method=route,batch_seconds=round(time.monotonic()-t,2),elapsed=round(time.monotonic()-started,1))),flush=True)
            write_json(provenance/'progress.json',dict(done=done,total=len(rows),last_key=key,elapsed_seconds=time.monotonic()-started,complete=done==len(rows)))
    # Re-audit the entire candidate pack, including resumed pairs.
    total=0; counts={}
    for dds in output.glob('*.dds'):
        m=json.loads(dds.with_suffix('.json').read_text()); total+=audit.check_asset(dds,m,entries[dds.stem]); counts[m['method']]=counts.get(m['method'],0)+1
    report=dict(candidate_count=sum(counts.values()),routes=counts,dds_bytes=total,selected_count=len(rows),
                full_scope_complete=sum(counts.values())==plan['candidate_count'],scene_validation='pending',storage=storage(root))
    write_json(provenance/'audit.json',report); print(json.dumps(report,indent=2),flush=True)


@contextmanager
def candidate_lock(output):
    parent=output.resolve().parent
    parent.mkdir(parents=True,exist_ok=True)
    lock=parent/'production.lock'
    try:
        with lock.open('x',encoding='utf-8') as f: f.write(str(os.getpid()))
    except FileExistsError:
        raise ValueError('Production lock exists; inspect the owner before resuming: '+str(lock))
    try: yield
    finally: lock.unlink()


def run(args):
    root=args.root.resolve(); output=args.output.resolve()
    if not output.is_relative_to(root/'authored'):
        raise ValueError('Candidates must be under authored/')
    with candidate_lock(output): _run(args)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,required=True); p.add_argument('--game',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True); p.add_argument('--exe',type=Path,required=True)
    p.add_argument('--models',type=Path,required=True); p.add_argument('--gpu',type=int,default=0)
    p.add_argument('--policy',type=Path,default=REPO/'config/textures/texture-originals.json')
    p.add_argument('--keys',nargs='+'); p.add_argument('--compare',nargs='+'); p.add_argument('--limit',type=int)
    p.add_argument('--review-overrides',type=Path,help='Reviewed per-key conservative fallback decisions; never enables new AI categories')
    p.add_argument('--plan-only',action='store_true')
    a=p.parse_args()
    try: run(a)
    except (ValueError,OSError,KeyError,subprocess.TimeoutExpired) as e: p.exit(2,str(e)+'\n')


if __name__=='__main__':main()
