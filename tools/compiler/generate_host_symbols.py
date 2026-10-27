"""Regenerate factual host-export names from installed Windows SDK import libs.

The output contains identifiers only, not SDK library contents. The manifest
records relative input names and SHA-256 digests for reproducible provenance.
Run explicitly with a dumpbin executable and the SDK's um/x64 library folder.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess


def exports(text):
    active=False; names=set()
    for line in text.splitlines():
        if line.strip()=='ordinal    name': active=True; continue
        if active and line and not line[0].isspace(): active=False
        if active and re.fullmatch(r'\s{10,}[A-Za-z_][A-Za-z_0-9]*\s*',line):
            names.add(line.strip())
    return names


def generate(dumpbin, libraries, destination, manifest):
    inputs=sorted(libraries.glob('*.lib'),key=lambda p:p.name.lower())
    if not inputs: raise ValueError('No import libraries in selected SDK directory')
    names=set(); records=[]
    for path in inputs:
        result=subprocess.run([str(dumpbin),'/nologo','/exports',str(path)],
            capture_output=True,text=True,errors='replace',timeout=60)
        if result.returncode: raise RuntimeError(f'Cannot inspect {path.name}: {result.stderr}')
        symbols=exports(result.stdout); names.update(symbols)
        with path.open('rb') as stream: digest=hashlib.file_digest(stream,'sha256').hexdigest()
        records.append(dict(name=path.name,sha256=digest,export_identifiers=len(symbols)))
    if not {'CreateThread','MessageBoxW','GetLastError'}<=names:
        raise ValueError('SDK export inventory is incomplete')
    content='\n'.join(sorted(names))+'\n'
    destination.write_text(content,encoding='ascii',newline='\n')
    manifest.write_text(json.dumps(dict(format=1,source='Windows SDK um/x64 import-library exports',
        sdk_version=libraries.parent.parent.name,tool='Microsoft dumpbin /nologo /exports',
        symbol_count=len(names),output_sha256=hashlib.sha256(content.encode('ascii')).hexdigest(),
        libraries=records),indent=2)+'\n',encoding='utf-8',newline='\n')
    return len(names)


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('dumpbin','libraries','output','manifest'):parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    print(f'Wrote {generate(args.dumpbin,args.libraries,args.output,args.manifest)} SDK export identifiers')
