"""Build deterministic game UI spans from a PNG input-icon manifest."""
import argparse
import json
import hashlib
from pathlib import Path
from PIL import Image
ROOT = Path(__file__).resolve().parents[2]

def rectangles(image):
    """Losslessly merge identical RGBA pixel runs, including antialiased edges."""
    pixels = image.convert('RGBA'); result = []; previous = {}
    for y in range(pixels.height):
        active = {}; x = 0
        while x < pixels.width:
            color = pixels.getpixel((x, y)); end = x + 1
            while end < pixels.width and pixels.getpixel((end, y)) == color:
                end += 1
            if color[3]:
                key = (x, end-x, color)
                if key in previous:
                    index = previous[key]; result[index][3] += 1
                else:
                    index = len(result); result.append([x, y, end-x, 1, *color])
                active[key] = index
            x = end
        previous = active
    return result


def compile_pack(manifest, output, source_dir=None):
    config = json.loads(manifest.read_text(encoding='utf-8'))
    source_dir = (source_dir or manifest.parent).resolve()
    shapes = []; icons = []; lookup = [-1]*256; controller_lookup = [-1]*256; images = {}
    for entry in config['keys'] + config.get('controller', []):
        target = controller_lookup if 'token' in entry else lookup
        key = entry.get('token', entry.get('key'))
        if not isinstance(key,int) or not 0<=key<256 or target[key]>=0:
            raise ValueError('Invalid or duplicated virtual key')
        target[key]=len(icons)//2
        for state in ('idle','pressed'):
            relative=entry.get(state,entry['idle'])
            image_path=(source_dir/relative).resolve()
            if not image_path.is_relative_to(source_dir):
                raise ValueError('Icon path escapes the asset folder')
            if state=='idle' and entry.get('sha256') and hashlib.sha256(image_path.read_bytes()).hexdigest()!=entry['sha256']:
                raise ValueError('Source icon SHA-256 differs from manifest')
            if image_path in images:
                icons.append(images[image_path]);continue
            with Image.open(image_path) as source:
                if source.size!=(128,128):
                    raise ValueError('Expected 128px square source icons')
                rgba=source.convert('RGBA')
                bounds=rgba.getchannel('A').point(lambda a:255 if a>=64 else 0).getbbox()
                scale=min(2.,max(1.,104./(bounds[3]-bounds[1]))) if bounds else 1.
                image=rgba.resize((32,32),Image.Resampling.LANCZOS)
            data=rectangles(image);image_range=(len(shapes),len(data),scale)
            images[image_path]=image_range;icons.append(image_range);shapes.extend(data)
    if not icons: raise ValueError('No icons in manifest')
    text=['/* Input artwork: see assets/ui/input-icons/ATTRIBUTION.md.',
          ' * Generated from assets/ui/input-icons/manifest.json.',
          ' * Attribution and modifications: assets/ui/input-icons/ATTRIBUTION.md. */',
          '#define TSFP_INPUT_ICONS 1',
          'typedef struct PcInputIconSpan { unsigned char x,y,w,h,r,g,b,a; } PcInputIconSpan;',
          'static const PcInputIconSpan input_icon_spans[]={']
    text += ['{'+','.join(map(str,shape))+'},' for shape in shapes]
    text += ['};','typedef struct PcInputIconImage { unsigned first,count; float scale; } PcInputIconImage;',
             'static const PcInputIconImage input_icons[]={']
    text += ['{%d,%d,%.6ff},'%image for image in icons]
    text += ['};','static const short input_icon_lookup[256]={',','.join(map(str,lookup)),'};',
             '#define TSFP_INPUT_ICON_METRICS 1',
             '#define TSFP_CONTROLLER_ICONS 1',
             'static const short input_controller_lookup[256]={',','.join(map(str,controller_lookup)),'};','']
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text('\n'.join(text),encoding='utf-8',newline='\n')
    return len(shapes)

if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--manifest',type=Path,default=ROOT/'assets/ui/input-icons/manifest.json')
    parser.add_argument('--output',type=Path,default=ROOT/'src/interface/pc_input_icon_data.inc')
    parser.add_argument('--source-dir',type=Path,help='Directory containing the licensed source PNGs')
    args=parser.parse_args()
    print(f'Imported {compile_pack(args.manifest,args.output,args.source_dir)} merged spans into {args.output}')
