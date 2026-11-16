"""Private due-time backup rotation; only this newly initialized pool is managed."""
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import re
import sqlite3
import stat
import time
import uuid

from mod_backup import (MAX_TOTAL, MANIFEST, backup_manifest, private_directory, regular,
                        snapshot, sync_directory, verify_snapshot, write_json)
from mod_package import ModError, read_json, require

POLICY='pool.json'
LOCK='.pool.lock'
PRODUCER='tsfp-managed-backup-pool-v1'
MAX_SNAPSHOTS=64
MANIFEST_RESERVE=4*1024**2


def initialize(repository, pool, *, keep=7, interval=86400, maximum_bytes=MAX_TOTAL):
    repository=private_directory(repository)
    require(type(keep) is int and 1<=keep<=32, 'Keep between1 and32 managed backups')
    require(type(interval) is int and 3600<=interval<=604800, 'Use a backup interval between1 hour and7 days')
    require(type(maximum_bytes) is int and MANIFEST_RESERVE<maximum_bytes<=MAX_TOTAL, 'Invalid managed storage budget')
    pool=Path(os.path.abspath(pool));private_directory(pool.parent)
    require(not pool.is_relative_to(repository) and not repository.is_relative_to(pool), 'Separate managed backups and repository')
    require(not pool.exists() and not pool.is_symlink(), 'Preserving existing backup pool path')
    pool.mkdir(mode=0o700)
    write_json(pool/POLICY,dict(schema=1,producer=PRODUCER,repository=str(repository),
        pool_id=uuid.uuid4().hex,keep=keep,interval=interval,maximum_bytes=maximum_bytes))
    sync_directory(pool);sync_directory(pool.parent)
    return dict(pool=str(pool),keep=keep,interval=interval,maximum_bytes=maximum_bytes)


def policy(pool):
    pool=private_directory(pool)
    path=pool/POLICY
    info=path.lstat()
    require(stat.S_ISREG(info.st_mode) and info.st_nlink==1 and info.st_uid==os.geteuid()
            and info.st_mode&0o077==0 and 0<info.st_size<=8192, 'Unsafe managed backup policy')
    value=read_json(path.read_bytes())
    require(isinstance(value,dict) and set(value)=={'schema','producer','repository','pool_id','keep','interval','maximum_bytes'}
            and type(value['schema']) is int and value['schema']==1 and value['producer']==PRODUCER,
            'Unknown managed backup policy')
    require(isinstance(value['repository'],str) and Path(value['repository']).is_absolute()
            and isinstance(value['pool_id'],str) and re.fullmatch('[0-9a-f]{32}',value['pool_id']), 'Invalid pool identity')
    require(type(value['keep']) is int and 1<=value['keep']<=32
            and type(value['interval']) is int and 3600<=value['interval']<=604800
            and type(value['maximum_bytes']) is int and MANIFEST_RESERVE<value['maximum_bytes']<=MAX_TOTAL, 'Invalid backup pool limits')
    repository=private_directory(value['repository'])
    require(not pool.is_relative_to(repository) and not repository.is_relative_to(pool), 'Separate managed backups and repository')
    return pool,value


@contextmanager
def locked(pool):
    import fcntl
    fd=os.open(pool/LOCK,os.O_CREAT|os.O_RDWR|os.O_NOFOLLOW|os.O_NONBLOCK,0o600)
    try:
        info=os.fstat(fd)
        require(stat.S_ISREG(info.st_mode) and info.st_nlink==1 and info.st_uid==os.geteuid()
                and info.st_mode&0o077==0 and info.st_size==0, 'Unsafe managed backup lock')
        try:fcntl.flock(fd,fcntl.LOCK_EX|fcntl.LOCK_NB)
        except BlockingIOError as error:raise ModError('Managed backup pool is busy') from error
        yield
    finally:os.close(fd)


def entries(pool, settings):
    result=[];total=0
    pattern=re.compile(r'snapshot-'+settings['pool_id']+r'-[0-9]{10,12}-[0-9a-f]{8}\Z')
    for index,entry in enumerate(pool.iterdir()):
        require(index<MAX_SNAPSHOTS+2, 'Managed backup directory budget exceeded')
        if entry.name in (POLICY,LOCK):continue
        require(pattern.fullmatch(entry.name) and not entry.is_symlink(), 'Preserving unexpected managed pool entry')
        source,manifest,size=backup_manifest(entry)
        private_directory(source/'blobs');private_directory(source/'incoming')
        for name,record in manifest['files'].items():
            require(regular(source/name,record['size']).st_size==record['size'], 'Managed snapshot size changed')
        require(type(manifest['created']) is int and 0<=manifest['created']<10**12, 'Invalid managed snapshot date')
        total+=size+(source/MANIFEST).stat().st_size
        require(total<=settings['maximum_bytes'], 'Managed backup storage budget exceeded')
        result.append(dict(path=source,created=manifest['created'],bytes=size))
    return sorted(result,key=lambda item:(item['created'],int(item['path'].name.rsplit('-',1)[1],16))),total


def retain(pool, settings, current, verified_path):
    require(current and current[-1]['path']==verified_path, 'Newest verified backup must survive retention')
    removed=[]
    for candidate in current[:-settings['keep']]:
        require(candidate['path']!=verified_path, 'New backup must survive retention')
        remove_verified(pool,candidate);removed.append(candidate['path'].name)
    return removed


def remove_verified(pool, candidate):
    """Unlink only the exact verified three-level snapshot, never recurse/follow."""
    source=candidate['path']
    require(source.parent==pool, 'Managed snapshot escaped pool')
    verify_snapshot(source)
    _,manifest,_=backup_manifest(source)
    parent=os.open(pool,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW)
    child=None
    try:
        child=os.open(source.name,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW,dir_fd=parent)
        require(os.fstat(child).st_ino==source.lstat().st_ino, 'Managed snapshot changed before retention')
        for directory in ('blobs','incoming'):
            fd=os.open(directory,os.O_RDONLY|os.O_DIRECTORY|os.O_NOFOLLOW,dir_fd=child)
            try:
                for name in sorted(manifest['files']):
                    if name.startswith(directory+'/'):os.unlink(name.split('/')[1],dir_fd=fd)
                os.fsync(fd)
            finally:os.close(fd)
            os.rmdir(directory,dir_fd=child)
        os.unlink('catalog.sqlite',dir_fd=child);os.unlink(MANIFEST,dir_fd=child)
        os.fsync(child);os.close(child);child=None
        os.rmdir(source.name,dir_fd=parent);os.fsync(parent)
    finally:
        if child is not None:os.close(child)
        os.close(parent)


def run(pool, *, force=False, clock=time.time):
    pool,settings=policy(pool)
    with locked(pool):
        current,total=entries(pool,settings)
        now=int(clock())
        require(0<=now<10**12, 'Invalid backup clock')
        require(not current or now>=current[-1]['created'], 'Backup clock moved backwards')
        removed=[]
        # A crash after manifest commit needs no mutable journal: recognize the
        # complete pool-owned snapshot and finish its verified rotation first.
        if len(current)>settings['keep']:
            verify_snapshot(current[-1]['path'])
            removed=retain(pool,settings,current,current[-1]['path'])
            current,total=entries(pool,settings)
        if current and not force and now-current[-1]['created']<settings['interval']:
            return dict(status='not-due',snapshots=len(current),bytes=total,removed=removed,next_due=current[-1]['created']+settings['interval'])
        require(len(current)<MAX_SNAPSHOTS, 'Managed snapshot count requires operator inspection')
        available=settings['maximum_bytes']-total-MANIFEST_RESERVE
        require(available>0, 'Managed backup capacity exhausted; previous backups preserved')
        sequence=max((int(item['path'].name.rsplit('-',1)[1],16) for item in current),default=0)+1
        require(sequence<=0xffffffff, 'Managed snapshot sequence exhausted')
        target=pool/('snapshot-'+settings['pool_id']+'-'+str(now).zfill(10)+'-'+format(sequence,'08x'))
        result=snapshot(settings['repository'],target,maximum_total=available)
        verified=verify_snapshot(target)
        require(verified['bytes']==result['bytes'], 'New snapshot differs after capture')
        require(now<=verified['created']<=int(clock()), 'Snapshot clock differs from scheduler')
        current,total=entries(pool,settings)
        removed+=retain(pool,settings,current,target)
        retained,total=entries(pool,settings)
        return dict(status='captured',backup=target.name,snapshots=len(retained),bytes=total,
                    removed=removed,verified_manifest_sha256=verified['manifest_sha256'],
                    next_due=verified['created']+settings['interval'])


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    commands=parser.add_subparsers(dest='action',required=True)
    create=commands.add_parser('init');create.add_argument('repository');create.add_argument('pool')
    create.add_argument('--keep',type=int,default=7);create.add_argument('--interval',type=int,default=86400)
    create.add_argument('--maximum-bytes',type=int,default=MAX_TOTAL)
    tick=commands.add_parser('run');tick.add_argument('pool');tick.add_argument('--force',action='store_true')
    args=parser.parse_args()
    try:
        value=(initialize(args.repository,args.pool,keep=args.keep,interval=args.interval,maximum_bytes=args.maximum_bytes)
               if args.action=='init' else run(args.pool,force=args.force))
        print(json.dumps(value,sort_keys=True));return 0
    except (ModError,OSError,ValueError,sqlite3.Error) as error:
        print('Managed backups: '+str(error),file=__import__('sys').stderr);return 2


if __name__=='__main__':
    raise SystemExit(main())
