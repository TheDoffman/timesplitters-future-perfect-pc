"""Resolve documented MSVC compiler exceptions by symbol, never chunk number."""
import argparse
import json
from pathlib import Path
import re
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/compiler'))
from c_source_scan import mask_noncode,strip_disabled

NAME=re.compile(r'recomp_(?:context_)?\d{4,}\.c\Z')
SYMBOL=re.compile(r'(?:sub_[0-9A-F]{8}(?:_gen)?|tsfp_context_run_[0-9A-F]{8})\Z')


def resolve(directory, manifest):
    if manifest.get('version')!=1 or manifest.get('title_id')!='45410066':
        raise ValueError('Unsupported MSVC generated-code policy')
    rules={}
    for rule in manifest['rules']:
        symbol,options=rule['symbol'],rule['options']
        if not SYMBOL.fullmatch(symbol) or symbol in rules or options not in (['/Ob1'],['/Od','/Ob0']):
            raise ValueError('Invalid or duplicate MSVC compiler rule')
        rules[symbol]=options
    hits={symbol:[] for symbol in rules}; units={}
    # Most files contain none of these names; tokenize only candidate definitions.
    declarations=re.compile(r'^(?:void|int)\s+('+ '|'.join(map(re.escape,rules))+r')\s*\(',re.M)
    for path in sorted(Path(directory).glob('*.c')):
        if not NAME.fullmatch(path.name):continue
        if path.is_symlink():raise ValueError('Generated compiler input must not be a symlink')
        text=path.read_text(encoding='utf-8')
        candidates={m[1] for m in declarations.finditer(text)}
        if not candidates:continue
        live=mask_noncode(strip_disabled(text))
        pattern=r'^(?:void|int)\s+('+ '|'.join(map(re.escape,rules))+r')\s*\([^;{}\n]*\)\s*\{'
        definitions={m[1] for m in re.finditer(pattern,live,re.M)}
        for symbol in sorted(candidates & definitions):
            hits[symbol].append(path.name)
            units.setdefault(path.name,[]).append(symbol)
    invalid={symbol:files for symbol,files in hits.items() if len(files)!=1}
    if invalid:raise ValueError(f'MSVC exception symbols must each have one definition: {invalid}')
    result=[]
    for name,symbols in sorted(units.items()):
        options=['/Od','/Ob0'] if any(rules[s]==['/Od','/Ob0'] for s in symbols) else ['/Ob1']
        result.append(dict(source=name,symbols=symbols,options=options))
    return result


def publish(directory,manifest,output):
    rows=resolve(directory,manifest)
    lines=['# Generated MSVC compatibility policy; generated guest C is untouched.']
    for row in rows:
        lines.append('set_property(SOURCE "${TSFP_MSVC_GENERATED_DIR}/'+row['source']+
                     '" APPEND PROPERTY COMPILE_OPTIONS '+
                     ' '.join('"$<$<NOT:$<CONFIG:Debug>>:'+option+'>"' for option in row['options'])+')')
    output=Path(output);output.parent.mkdir(parents=True,exist_ok=True)
    text='\n'.join(lines)+'\n'
    if not output.exists() or output.read_text(encoding='utf-8')!=text:
        output.write_text(text,encoding='utf-8')
    output.with_suffix('.json').write_text(json.dumps(rows,indent=2)+'\n',encoding='utf-8')
    return rows


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--generated-dir',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    manifest=json.loads((ROOT/'config/build/msvc-generated-code.json').read_text(encoding='utf-8'))
    rows=publish(args.generated_dir,manifest,args.output)
    print(f'Applied {len(rows)} symbol-based MSVC compatibility rules; other source retains Release optimization.')
