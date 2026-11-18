"""Seal verified reboot layout metadata; never grants ordinary effect authority."""
import hashlib
from itertools import islice
import os
import re
import stat

import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
import mod_host_reboot_layout as layout
from mod_host_migration import boot_identity
from mod_package import read_json,require

MAX_CHECKPOINTS=32
MAX_BYTES=65536


def selection(journal):
    state=journal.state();reboot=state['reboot']
    require(os.geteuid()==cutover.ROOT_UID and reboot is not None and reboot['quiescent']
            and journal.observation_boot==boot_identity()==reboot['boot_id'] and state['layout_sha256'] is not None,
            'Seal reboot evidence only under current stopped Root authority')
    return dict(boot_id=reboot['boot_id'],original_boot=journal.base['boot_id'],nonce=journal.base['nonce'],
                layout_sha256=state['layout_sha256'],context_sha256=reboot['context_sha256'])


def name(binding):return binding['nonce']+'-'+binding['boot_id']+'.json'


def directory(journal,*,create=False):
    selection(journal);folder=cutover.ROOT/'reboot-checkpoints'
    if create:
        parent=cutover.protected_directory(cutover.ROOT)
        try:
            try:os.mkdir(folder.name,0o700,dir_fd=parent);os.fsync(parent)
            except FileExistsError:pass
        finally:os.close(parent)
    fd=cutover.protected_directory(folder)
    try:
        require(stat.S_IMODE(os.fstat(fd).st_mode)==0o700,'Unsafe reboot checkpoint storage')
        with os.scandir(fd) as scan:names=[entry.name for entry in islice(scan,MAX_CHECKPOINTS+1)]
        require(len(names)<=MAX_CHECKPOINTS and all(re.fullmatch('[0-9a-f]{64}-[0-9a-f]{64}\\.json',member) for member in names),
                'Preserving unknown or excessive reboot checkpoint artifacts')
        return folder,fd,names
    except BaseException:os.close(fd);raise


def checked(data,binding):
    value=read_json(data)
    require(0<len(data)<=MAX_BYTES and data==storage.wire(value) and isinstance(value,dict)
            and set(value)=={'schema','phase',*binding,'locations','inventories','services_started','files_changed','operational_admission'}
            and type(value['schema']) is int and value['schema']==1 and value['phase']=='reboot-layout-observed'
            and all(value[key]==expected for key,expected in binding.items())
            and value['services_started'] is value['files_changed'] is value['operational_admission'] is False
            and isinstance(value['locations'],dict) and set(value['locations'])=={'active','saved','staged'}
            and all(side in (None,'before','after') for side in value['locations'].values())
            and sorted(side for side in value['locations'].values() if side is not None)==['after','before']
            and value['locations']['saved'] in (None,'before') and value['locations']['staged'] in (None,'after')
            and isinstance(value['inventories'],dict) and set(value['inventories'])=={key for key,side in value['locations'].items() if side is not None},
            'Reboot checkpoint differs from its exact observation binding')
    return value


def load(journal,expected):
    binding=selection(journal);receipt=journal.state()['reboot']['layout_checkpoint']
    require(isinstance(expected,str) and storage.SHA.fullmatch(expected)
            and receipt==dict(binding,checkpoint_sha256=expected),'Reboot checkpoint lacks its journaled hash')
    folder,fd,_=directory(journal)
    try:
        data,_=cutover.protected_bytes(folder/name(binding),0o600,MAX_BYTES)
        require(hashlib.sha256(data).hexdigest()==expected,'Reboot checkpoint bytes changed')
        return checked(data,binding)
    finally:os.close(fd)


def seal(journal,fresh,*,resume=False):
    require(type(resume) is bool,'Choose explicit reboot checkpoint recovery')
    binding=selection(journal);report=layout.read(journal,fresh);data=storage.wire(report)
    checked(data,binding);sha=hashlib.sha256(data).hexdigest()
    folder,fd,names=directory(journal,create=True);member=name(binding)
    try:
        if member in names:
            require(resume and cutover.protected_bytes(folder/member,0o600,MAX_BYTES)[0]==data,
                    'Preserving existing or partial reboot checkpoint')
        else:
            require(len(names)<MAX_CHECKPOINTS,'Reboot checkpoint capacity exhausted')
            child=os.open(member,os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW,0o600,dir_fd=fd)
            with os.fdopen(child,'wb') as stream:
                stream.write(data);stream.flush();os.fchmod(stream.fileno(),0o600);os.fsync(stream.fileno())
            os.fsync(fd)
        visible=cutover.protected_directory(folder)
        try:require((os.fstat(fd).st_dev,os.fstat(fd).st_ino)==(os.fstat(visible).st_dev,os.fstat(visible).st_ino),'Reboot checkpoint directory changed')
        finally:os.close(visible)
        require(cutover.protected_bytes(folder/member,0o600,MAX_BYTES)[0]==data,'Reboot checkpoint changed before admission')
        require(storage.wire(layout.read(journal,fresh))==data,'Reboot layout changed before checkpoint admission')
        receipt=dict(binding,checkpoint_sha256=sha);recorded=journal.state()['reboot']['layout_checkpoint']
        if recorded is None:journal.record_reboot('reboot-layout-verified',data=receipt)
        else:require(resume and recorded==receipt,'Preserving a different recorded reboot checkpoint')
        load(journal,sha)
    finally:os.close(fd)
    return dict(schema=1,phase='reboot-layout-checkpoint-sealed',**receipt,operational_admission=False)
