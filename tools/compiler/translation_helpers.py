"""Project recognition of bounded MSVC guest exception and nonlocal-jump helpers.

These byte signatures describe the guest ABI, not host exception support.
Recognition is heuristic; it does not prove that an arbitrary function is safe
to replace, and the returned addresses still execute through the normal runtime.
"""
import struct
import sys
import translation_layout as layout


def _extent(address, info, *, end_only=False):
    if not end_only:
        try: size=int(info.get('size') or 0)
        except (TypeError,ValueError,OverflowError): size=0
        if size: return size
    end=info.get('end')
    if isinstance(end,str):
        try: end=int(end,16)
        except ValueError: return 0
    return end-address if isinstance(end,int) else 0


def _read(image, address, size):
    if image is None or not isinstance(address,int) or size<=0: return None
    offset=layout.va_to_file_offset(address)
    if offset is None or offset<0 or offset+size>len(image): return None
    cursor=address
    while cursor<address+size:
        section=layout._section_at(cursor)
        if section is None or layout.va_to_file_offset(cursor)!=offset+cursor-address:return None
        stop=min(address+size,section.va+min(section.va_size,section.raw_size))
        if stop<=cursor:return None
        cursor=stop
    return memoryview(image)[offset:offset+size]


def _bodies(functions, image, limit, *, end_only=False):
    for address,info in sorted((functions or {}).items()):
        size=_extent(address,info,end_only=end_only)
        if 0<size<=limit:
            body=_read(image,address,size)
            if body is not None: yield address,bytes(body)


def exception_record(image, address):
    record=_read(image,address,32)
    return (record is not None and struct.unpack_from('<I',record,0)[0]==0xe06d7363
            and struct.unpack_from('<II',record,16)==(3,0x19930520))


def seh(functions, image, verbose=False):
    prologs=[];epilogs=[]
    # mov eax,fs:[0]; lea ebp,[esp+12 or 16]. Epilog restores fs:[0]
    # from ECX and ends the frame with leave; push ecx; ret.
    read_chain=bytes.fromhex('64a100000000')
    restore_chain=bytes.fromhex('64890d00000000')
    frames=(bytes.fromhex('8d6c240c'),bytes.fromhex('8d6c2410'))
    unwind=bytes.fromhex('c951c3')
    for address,body in _bodies(functions,image,128):
        if read_chain in body and any(frame in body for frame in frames):prologs.append(address)
        elif len(body)<=64 and restore_chain in body and unwind in body:epilogs.append(address)
    if verbose:
        display=lambda values:', '.join(f'0x{v:08X}' for v in values) or 'not found'
        print(f'  SEH helpers: prologs {display(prologs)}, epilogs {display(epilogs)}',file=sys.stderr)
    return tuple(prologs),tuple(epilogs)


def cxx_throw(functions, image, verbose=False):
    records={}
    for address,body in _bodies(functions,image,256):
        # Candidate pointers may be unaligned instruction immediates. Cache
        # record validation across functions instead of repeatedly reading it.
        for offset in range(len(body)-3):
            pointer=struct.unpack_from('<I',body,offset)[0]
            if pointer not in records: records[pointer]=bool(pointer) and exception_record(image,pointer)
            if records[pointer]:
                if verbose:print(f'  _CxxThrowException: 0x{address:08X} (record template at 0x{pointer:08X})',file=sys.stderr)
                return address
    if verbose:print('  _CxxThrowException: not found (this title may not use C++ exceptions)',file=sys.stderr)
    return None


def nonlocal_jumps(functions, image, verbose=False):
    # Microsoft VC buffer version marker: MOV [EDX+32],0x56433230 / CMP EAX,marker.
    signatures=(bytes.fromhex('c7422030324356'),bytes.fromhex('3d30324356'))
    found=[None,None]
    for address,body in _bodies(functions,image,512,end_only=True):
        for index,signature in enumerate(signatures):
            if found[index] is None and signature in body:found[index]=address
        if all(value is not None for value in found):break
    if verbose:
        display=lambda value:'not found' if value is None else f'0x{value:08X}'
        print(f'  CRT non-local jumps: setjmp {display(found[0])}, longjmp {display(found[1])}',file=sys.stderr)
    return tuple(found)
