"""Publish original continuation C chunks and their exact decoded-PC registry."""
from collections import Counter
from copy import copy
import hashlib
import json
from pathlib import Path
import re
import tempfile

from continuation_index import InstructionIndex
from generation_output import write_if_changed
from translation_continuation import _decode,emit

_OWNED=re.compile(r'recomp_context_(?:[0-9]{4,}|dispatch)\.c\Z')
MANIFEST='recomp_context_manifest.json'
# Count alone does not bound compiler work: a few decoded SIMD bodies can be
# larger than hundreds of ordinary functions. Never split an individual body.
MAX_CHUNK_BODY_BYTES=512*1024


def _check_target(directory,name):
    path=directory/name
    if path.is_symlink() or path.resolve().parent!=directory:
        raise ValueError('Refusing continuation publication outside output directory')
    return path


def publish(translator,functions,directory,*,chunk_size=100,kept=()):
    """Generate before publishing; generation errors leave prior output untouched.

    Unsupported bodies are reported and excluded, never emitted as successful
    no-ops. Existing manual-only entries belong to the native adapter. Kept
    originals have real translated bodies, so their instruction entries remain
    available for resumption. This does not implement native wrapper lifetimes.
    """
    if type(chunk_size) is not int or chunk_size<=0:
        raise ValueError('Continuation chunk size must be positive')
    directory=Path(directory).resolve();functions=sorted(functions)
    if len({a for a,_ in functions})!=len(functions):
        raise ValueError('Duplicate continuation function entry')
    kept=set(kept);manual=set(translator.lifter.manual_functions)
    clone=copy(translator);clone.lifter=copy(translator.lifter)
    clone.lifter.manual_functions=manual-kept
    index=InstructionIndex();rejected=[];opaque=[];pending=[];chunks=[];count=lines=0
    pending_bytes=0
    directory.mkdir(parents=True,exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.context-generation-',dir=directory) as temporary:
        staging=Path(temporary)
        def flush():
            nonlocal pending_bytes
            if not pending:return
            name=f'recomp_context_{len(chunks):04d}.c'
            text='\n'.join(['/* Original decoded instruction continuations. */',
                '#define RECOMP_GENERATED_CODE','#define RECOMP_GPR_REFS_SUPPORTED 1',
                '#include "guest_context_execution.h"','#include "recomp_types.h"',*pending])
            write_if_changed(staging/name,text);chunks.append(name);pending.clear();pending_bytes=0
        for address,info in functions:
            if address in manual and address not in kept:
                opaque.append(address);continue
            try:
                lifter,instructions=_decode(clone,address,info)
                if not instructions:raise ValueError('No decoded continuation body')
                text=emit(address,instructions,lifter,run=True)
            except ValueError as error:
                rejected.append({'address':address,'reason':str(error)});continue
            # Inconsistent ownership/bytes are fatal, not a per-function fallback.
            index.add(address,instructions)
            body_bytes=len(text.encode('utf-8'))
            if pending and pending_bytes+body_bytes>MAX_CHUNK_BODY_BYTES:flush()
            pending.append(text);count+=1;lines+=text.count('\n')
            pending_bytes+=body_bytes
            if len(pending)==chunk_size or pending_bytes>=MAX_CHUNK_BODY_BYTES:flush()
        flush()
        if index.functions:
            dispatch=index.emit(run=True)
        else:
            dispatch='''#include "guest_context_execution.h"
int tsfp_context_generated_admit(uint32_t pc){(void)pc;return 0;}
int tsfp_context_generated_step(uint32_t pc,uint32_t *next){(void)pc;(void)next;return TSFP_CONTEXT_STEP_ERROR;}
'''
        dispatch_name='recomp_context_dispatch.c'
        write_if_changed(staging/dispatch_name,dispatch)
        names=chunks+[dispatch_name]
        shared=Path(__file__).resolve().parents[2]/'src/runtime'
        for name in ('guest_context_execution.h','guest_x86_context.h'):
            write_if_changed(staging/name,(shared/name).read_text(encoding='utf-8'));names.append(name)
        # Validate every target, including stale cleanup, before first replacement.
        for name in names+[MANIFEST]:_check_target(directory,name)
        stale=[p for p in directory.iterdir() if _OWNED.fullmatch(p.name) and p.name not in names]
        for path in stale:_check_target(directory,path.name)
        hashes={name:hashlib.sha256((staging/name).read_text(encoding='utf-8').encode()).hexdigest() for name in names}
        reasons=Counter(re.sub(r' at 0x[0-9a-f]+','',row['reason']) for row in rejected)
        report={'version':1,'mode':'decoded-control-blocks','selected':len(functions),
                'functions':count,'instruction_entries':len(index.points),'shared_entries':index.shared,
                'opaque_entries':opaque,'rejected':rejected,'rejection_counts':dict(reasons),
                'chunks':len(chunks),'lines':lines+dispatch.count('\n'),'sha256_text':hashes}
        # Individual replacements are atomic. Manifest is published last; a
        # failed replacement never advertises the incomplete generation as done.
        for name in names:
            write_if_changed(_check_target(directory,name),(staging/name).read_text(encoding='utf-8'))
        for path in stale:path.unlink()
        write_if_changed(_check_target(directory,MANIFEST),json.dumps(report,indent=2)+'\n')
    return report
