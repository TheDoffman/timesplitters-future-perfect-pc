"""Selective workshop Funnel transitions; unrelated configuration is preserved."""
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
from urllib.parse import urlsplit

from mod_package import read_json,require

BINARY=Path('/Applications/Tailscale.app/Contents/MacOS/Tailscale')
TARGET='http://127.0.0.1:8768'
FIELDS=('TCP','Web','AllowFunnel')


def digest(value):
    return hashlib.sha256(json.dumps(value,sort_keys=True,separators=(',',':')).encode()).hexdigest()


def authority(origin):
    require(isinstance(origin,str) and len(origin)<=512 and origin.isascii(),'Invalid workshop route origin')
    address=urlsplit(origin)
    require(address.scheme=='https' and address.hostname and address.port==8443
            and not address.path and not address.query and not address.fragment
            and address.netloc==address.hostname+':8443','Route only the pinned workshop HTTPS port')
    return address.netloc


def snapshot(data,origin):
    """Return hashes/counts only; never log unrelated private proxy contents."""
    host=authority(origin)
    require(isinstance(data,bytes) and 0<len(data)<=65536,'Routing status exceeds budget')
    state=read_json(data)
    require(isinstance(state,dict) and not state.get('Foreground'),'Unsupported foreground routing state')
    require(all(state.get(key) is None or isinstance(state[key],dict) for key in FIELDS),'Invalid routing status maps')
    maps={key:state.get(key) or {} for key in FIELDS}
    for key in maps['TCP']:
        require(key.isascii() and key.isdigit() and 0<int(key)<=65535 and str(int(key))==key,
                'Ambiguous routing port')
    for field in ('Web','AllowFunnel'):
        require(all(not key.endswith(':8443') or key==host for key in maps[field]),
                'Another authority shares the workshop routing port')
    tcp=maps['TCP'].get('8443');web=maps['Web'].get(host);funnel=maps['AllowFunnel'].get(host)
    require(funnel is None or type(funnel) is bool,'Invalid workshop Funnel flag')
    present=tcp is not None or web is not None
    if present:
        require(tcp=={'HTTPS':True} and type(tcp.get('HTTPS')) is bool
                and web=={'Handlers':{'/':{'Proxy':TARGET}}},'Preserve an unknown workshop route')
        phase='public' if funnel is True else 'private'
    else:
        require(funnel is not True,'Funnel enabled without a workshop server')
        phase='absent'
    # Port443 is a separate private service on this host. Preserve its entire
    # configuration by hash and reject public exposure, even if pre-existing.
    require('443' in maps['TCP'] and not any(value is not False for key,value in maps['AllowFunnel'].items()
            if key.endswith(':443')),'The separate443 service must remain private')
    other=dict(state)
    for field in FIELDS:
        values=dict(maps[field]);values.pop('8443' if field=='TCP' else host,None)
        if values:other[field]=values
        else:other.pop(field,None)
    # Empty known maps are semantically absent. All unrelated values, including
    # private443 handlers and explicit false Funnel flags, otherwise stay exact.
    canonical=dict(other)
    for field,values in maps.items():
        if values:canonical[field]=values
        else:canonical.pop(field,None)
    return dict(schema=1,origin=origin,phase=phase,config_sha256=digest(canonical),unrelated_sha256=digest(other))


def transition(before,expose,invoke,record,*,resume=False):
    """Record durable intent before ONE action, then prove the resulting state.

    The coordinator supplies a root-owned saved public baseline and durable
    journal callback. It must verify private readiness before requesting restore.
    This primitive has no CLI or automatic activation/recovery authorization.
    """
    require(isinstance(before,dict) and set(before)=={'schema','origin','phase','config_sha256','unrelated_sha256'}
            and type(before['schema']) is int and before['schema']==1 and before['phase']=='public'
            and type(expose) is bool and type(resume) is bool,'Use the saved public workshop route baseline')
    require(all(isinstance(before[key],str) and len(before[key])==64
                and all(c in '0123456789abcdef' for c in before[key]) for key in ('config_sha256','unrelated_sha256')),
            'Invalid saved routing fingerprints')
    origin=before['origin'];authority(origin)
    current=snapshot(invoke('status'),origin)
    require(current['unrelated_sha256']==before['unrelated_sha256'],'Unrelated routing changed; preserving it')
    desired=current['phase']=='public' if expose else current['phase']!='public'
    if desired:
        require(resume,'Routing changed without this transition intent')
        record('routing-observed',dict(expose=expose,**current));return current
    if not expose:
        require(current['config_sha256']==before['config_sha256'],'Workshop route differs from its saved baseline')
    record('routing-intent',dict(expose=expose,**current))
    try:invoke('restore' if expose else 'suspend')
    except (OSError,ValueError,subprocess.SubprocessError):pass
    # An error/timeout can follow a successful OS change. Never reissue the
    # action on that basis: query this same configuration and preserve uncertainty.
    after=snapshot(invoke('status'),origin)
    require(after['unrelated_sha256']==before['unrelated_sha256']
            and ((after['phase']=='public') if expose else (after['phase']!='public')),
            'Workshop routing transition is unconfirmed; preserve the journal intent')
    if expose:require(after['config_sha256']==before['config_sha256'],'Restored workshop configuration differs')
    record('routing-observed',dict(expose=expose,**after))
    return after


def operator(before,after):
    """Fixed app CLI runs after dropping to the verified original operator.

    This app is operator-writable. It must never execute as root. The caller
    first verifies both release closures/authority and owns the routing journal.
    """
    require(sys.platform=='darwin' and os.geteuid()==0,'Routing operator transport requires the Mac coordinator')
    uid=before['descriptor']['owner_uid']
    require(type(uid) is int and uid>0 and uid==after['descriptor']['owner_uid']
            and uid!=before['uid'] and before['uid']==after['uid'],'Preserve the established routing operator identity')
    import pwd
    account=pwd.getpwuid(uid)
    require(account.pw_uid==uid and account.pw_gid>0 and Path(account.pw_dir).is_absolute(),'Invalid routing operator account')
    from mod_host_upgrade_process import _capture
    options=dict(user=uid,group=account.pw_gid,extra_groups=[],cwd=account.pw_dir,
        env=dict(PATH='/usr/bin:/bin:/usr/sbin:/sbin',HOME=account.pw_dir,LC_ALL='C',TZ='UTC'))
    commands=dict(status=['serve','status','--json'],suspend=['funnel','--bg','--https=8443','off'],
                  restore=['funnel','--bg','--https=8443','--yes',TARGET])
    def invoke(action):
        require(action in commands,'Invalid fixed routing action')
        return _capture([str(BINARY),*commands[action]],options,timeout=15,limit=65536)
    return invoke
