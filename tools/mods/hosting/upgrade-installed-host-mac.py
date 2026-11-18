"""Inspect, apply, recover and verify a preserving installed-host upgrade."""
import argparse
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
from urllib.parse import urlsplit


def helper(filename):
    if filename not in ('host_release.py','system-scan-monitor-mac.py'):
        raise ValueError('Invalid fixed upgrade helper')
    spec=importlib.util.spec_from_file_location('upgrade_'+filename.replace('.','_'),Path(__file__).with_name(filename))
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


release=helper('host_release.py')


def public_profile(verified):
    name='config/mods/public-workshop.json'
    release.require(name in verified['descriptor']['source_files'],'Installed source lacks a public workshop profile')
    value=release.decode(release.read_file(verified['root']/'source',name,release.ROOT_UID,8192),8192)
    release.require(isinstance(value,dict) and set(value)=={'schema','origin','trusted_root_sha256','status'}
        and type(value['schema']) is int and value['schema']==1 and value['status'] in ('active','pending-public-acceptance')
        and isinstance(value['trusted_root_sha256'],str) and release.SHA.fullmatch(value['trusted_root_sha256'])
        and isinstance(value['origin'],str) and 0<len(value['origin'])<=512 and value['origin'].isascii()
        and all(32<ord(c)<127 for c in value['origin']), 'Invalid installed public workshop profile')
    address=urlsplit(value['origin'])
    release.require(address.scheme=='https' and address.hostname and address.username is None
        and address.password is None and not address.path and not address.query and not address.fragment
        and (address.port is None or 1<=address.port<=65535),'Invalid installed public workshop origin')
    return value


def verified_pair(before,after,uid):
    release.require(isinstance(before,str) and isinstance(after,str) and release.SHA.fullmatch(before)
                    and release.SHA.fullmatch(after) and before!=after,'Choose two distinct exact installed releases')
    old=release.installed(before,uid);new=release.installed(after,uid)
    old_profile,new_profile=public_profile(old),public_profile(new)
    release.require(all(old_profile[k]==new_profile[k] for k in ('origin','trusted_root_sha256')),
                    'Upgrade changes the workshop authority; preserving it requires a separate contract')
    return old,new,old_profile


def inspection(before,after,uid):
    old,new,old_profile=verified_pair(before,after,uid)
    return dict(schema=1,phase='installed-upgrade-inspected',before=before,after=after,uid=uid,
        source_revisions=dict(before=old['descriptor']['source_revision'],after=new['descriptor']['source_revision']),
        origin=old_profile['origin'],trusted_root_sha256=old_profile['trusted_root_sha256'],
        installed_sources_verified=True,installed_runtimes_verified=True,
        services_changed=False,network_started=False,data_written=False)


def verified_inputs(before,after,uid,backup_identity):
    """Reverify closures/bindings and derive pure factories without private reads."""
    old,new,profile=verified_pair(before,after,uid)
    release.require(isinstance(backup_identity,str) and release.SHA.fullmatch(backup_identity),
                    'An exact preserved backup release is required')
    # Only verified new source is imported in this parent. Old host/backup
    # factories run in separate old-bound interpreters after credentials drop.
    required={'tools/mods/mod_host_upgrade_process.py','tools/mods/mod_host_upgrade_context.py','tools/mods/mod_host_cutover.py',
              'tools/mods/mod_host_cutover_journal.py','tools/mods/mod_host_upgrade_coordinator.py',
              'tools/mods/mod_host_upgrade_admission.py','tools/mods/mod_host_upgrade_activation.py',
              'tools/mods/mod_host_upgrade_layout.py','tools/mods/mod_host_upgrade_shutdown.py',
              'tools/mods/mod_host_migration.py','tools/mods/hosting/force-upgrade-backup-mac.py',
              'tools/mods/hosting/accept-installed-scanner-mac.py','tools/mods/hosting/accept-private-host-services-mac.py',
              'tools/mods/hosting/accept-public-host-services-mac.py',
              'tools/mods/mod_backup_release.py','tools/mods/mod_host_routing.py','tools/mods/hosting/upgrade-job-specs-mac.py',
              'tools/mods/hosting/migrate-private-generation-mac.py'}
    release.require(required <= new['descriptor']['source_files'].keys(),'Incomplete installed upgrade coordinator')
    sys.path.insert(0,str(new['root']/'source/tools/mods'))
    import mod_host_upgrade_process as process
    import mod_host_cutover as cutover
    import mod_backup_release as backup
    import mod_host_routing as routing
    from mod_host_watch import run_command
    folder=release.INSTALLED_ROOT/'backup-releases'/backup_identity
    _,binding=backup.installed(folder)
    release.require(binding['uid']==binding['gid']==uid,'Preserved backup service identity changed')
    runtime=release.installed(binding['base_release'],uid)
    release.require(binding['python']==str(runtime['python']),'Preserved backup runtime binding differs')
    invoke=routing.operator(old,new)
    host_jobs=process.job_specs(old,new,uid)
    backup_jobs=process.job_specs(runtime,new,uid,backup_identity=backup_identity)
    candidate_jobs=process.job_specs(new,new,uid)
    entries=host_jobs+backup_jobs
    candidate_entries=candidate_jobs+backup_jobs
    cutover.validate_entries(candidate_entries)
    return dict(old=old,new=new,profile=profile,backup_runtime=runtime,backup_binding=binding,
        operator=invoke,command=run_command,entries=entries,candidate_entries=candidate_entries)


def prepare_context(before,after,uid,backup_identity):
    """Retain initial private/live preflight after independently verifying inputs."""
    context=verified_inputs(before,after,uid,backup_identity)
    import mod_host_upgrade_process as process
    import mod_host_cutover as cutover
    import mod_host_routing as routing
    from mod_host_upgrade_context import preflight_digest
    old,new,profile=context['old'],context['new'],context['profile']
    route=routing.snapshot(context['operator']('status'),profile['origin'])
    release.require(route['phase']=='public','The workshop route requires explicit recovery before normal preflight')
    plan=process.migration_plan(new,uid,before,profile)
    private=process.private_readiness(old,uid,profile)
    observed=cutover.preflight(context['entries'],uid,context['command'])
    # Bind the candidate's factory outputs as well as the running old jobs.
    # Paths are explicitly encoded; no closure/callable/private payload enters
    # the report or its digest input.
    digest=preflight_digest(observed,context['candidate_entries'],route,plan,private)
    report=dict(schema=1,phase='installed-upgrade-preflight',before=before,after=after,uid=uid,
        origin=profile['origin'],trusted_root_sha256=profile['trusted_root_sha256'],
        backup_identity=backup_identity,backup_base_release=context['backup_runtime']['identity'],preflight_sha256=digest,
        plan_sha256=plan['sha256'],
        private_baseline=private,
        routing_sha256=route['config_sha256'],unrelated_routing_sha256=route['unrelated_sha256'],public_route_verified=True,
        jobs=10,protected_job_files=20,tracked_processes=len(observed['observation']['tracked']['pids']),
        services_changed=False,network_started=False,data_written=False)
    return dict(context,plan=plan,private_baseline=private,observed=observed,routing=route,report=report)


def prepare_recovery_context(before,after,uid,backup_identity,journal):
    """Reverify code/factories, then read historical metadata under the Root lease.

    Initial private inspection would reject a partially migrated configuration;
    recovery instead uses only the original journal-bound plan and baseline.
    """
    journal.state()
    release.require(all(journal.base[key]==value for key,value in
        (('before',before),('after',after),('uid',uid),('backup_identity',backup_identity))),
        'Recovery arguments differ from the held upgrade journal')
    fresh=verified_inputs(before,after,uid,backup_identity)
    from mod_host_upgrade_context import reconstruct
    return reconstruct(journal,fresh)


def preflight(before,after,uid,backup_identity):
    return prepare_context(before,after,uid,backup_identity)['report']


def run_action(before,after,uid,backup_identity,action,nonce,*,expected_preflight=None,resume_action=False):
    """Own the exact journal lease and sealed context before upgrade effects.

    Authentication/immutable entry-point admission precedes this call. Public
    restoration is separate and explicit. Recovery cannot create a new journal.
    """
    release.require(action in ('apply','resume','rollback','restore') and isinstance(nonce,str) and release.SHA.fullmatch(nonce)
        and type(resume_action) is bool and (not resume_action or action in ('rollback','restore'))
        and ((action=='apply' and isinstance(expected_preflight,str) and release.SHA.fullmatch(expected_preflight))
             or (action!='apply' and expected_preflight is None)), 'Choose an exact private upgrade action and transaction')
    # These calls verify both closures and the independent backup before any
    # coordinator/journal imports from the selected immutable new source.
    context=prepare_context(before,after,uid,backup_identity) if action=='apply' else None
    fresh=verified_inputs(before,after,uid,backup_identity) if context is None else None
    import mod_host_cutover as cutover
    import mod_host_cutover_journal as storage
    import mod_host_upgrade_context as contexts
    import mod_host_upgrade_coordinator as coordinator
    if action=='apply':
        release.require(context['report']['preflight_sha256']==expected_preflight,'Fresh preflight differs from the expected digest')
        arguments=dict(before=before,after=after,uid=uid,nonce=nonce,backup_identity=backup_identity,
            backup_base_release=context['backup_runtime']['identity'],plan_sha256=context['plan']['sha256'],
            preflight_sha256=expected_preflight,tracked=context['observed']['observation']['tracked'],routing=context['routing'])
    else:arguments=storage.recovery_arguments(nonce,before,after,uid,backup_identity)
    with storage.transaction(**arguments,new_only=action=='apply',existing_only=action!='apply',recover_pending=action!='apply') as journal:
        if action=='apply':
            cutover.job_files(context['entries'],context['observed']['files'])
            contexts.seal(journal,context)
        elif action=='resume' and not json.loads(journal.current)['events']:
            # A failed context write can leave a journal with no effects. Only
            # an exact initial re-preflight may complete/adopt its metadata;
            # partial or foreign context files remain refused by seal.
            context=prepare_context(before,after,uid,backup_identity)
            release.require(context['report']['preflight_sha256']==journal.base['preflight_sha256'],
                'Unused recovery transaction no longer matches initial preflight')
            contexts.seal(journal,context,resume=True)
        else:context=contexts.reconstruct(journal,fresh)
        if action=='restore':result=coordinator.restore_public(journal,context,resume=resume_action)
        elif action=='rollback':result=coordinator.rollback_private(journal,context,resume=resume_action)
        else:result=coordinator.apply_private(journal,context,resume=action=='resume')
        return dict(result,schema=1,nonce=nonce,before=before,after=after,uid=uid)


def root_context(before):
    release.require(isinstance(before,str) and release.SHA.fullmatch(before),'An exact installed base release is required')
    release.require(sys.platform=='darwin' and os.geteuid()==0,'Use local Mac administrator authentication')
    release.require(sys.flags.isolated and sys.flags.no_site and sys.dont_write_bytecode,
                    'Use installed Python with -I -S -B')
    after,account=helper('system-scan-monitor-mac.py').installed()
    script=Path(__file__).absolute()
    release.require(script==after/'source/tools/mods/hosting/upgrade-installed-host-mac.py',
                    'Use the immutable installed upgrade entry point')
    verified=release.installed(after.name,account['uid'])
    release.require(str(Path(sys.executable).resolve())==str(verified['python'])
                    and Path(sys.prefix).resolve().is_relative_to(after/'runtime')
                    and 'tools/mods/hosting/upgrade-installed-host-mac.py' in verified['descriptor']['source_files'],
                    'Upgrade entry point or Python does not belong to this installed release')
    return after.name,account['uid']


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    action=parser.add_mutually_exclusive_group()
    action.add_argument('--inspect',action='store_true')
    action.add_argument('--preflight',action='store_true')
    action.add_argument('--apply-private',action='store_true')
    action.add_argument('--resume-private',action='store_true')
    action.add_argument('--rollback-private',action='store_true')
    action.add_argument('--restore-public',action='store_true')
    parser.add_argument('--from-release')
    parser.add_argument('--backup-release')
    parser.add_argument('--nonce')
    parser.add_argument('--expected-preflight')
    parser.add_argument('--resume',action='store_true',help='Resume an already-started rollback or public restoration')
    args=parser.parse_args()
    try:
        private=args.apply_private or args.resume_private or args.rollback_private or args.restore_public
        if not (args.inspect or args.preflight or private):
            release.require(all(value is None for value in (args.from_release,args.backup_release,args.nonce,args.expected_preflight))
                and not args.resume,'Choose an explicit installed upgrade action')
            value=dict(schema=1,phase='plan-only',installed_releases=2,jobs=10,services_changed=False,
                       network_started=False,data_written=False)
        else:
            release.require(((args.preflight or private) and isinstance(args.backup_release,str) and release.SHA.fullmatch(args.backup_release))
                            or (args.inspect and args.backup_release is None),'Choose an exact preserved backup release')
            release.require((private and isinstance(args.nonce,str) and release.SHA.fullmatch(args.nonce))
                            or (not private and args.nonce is None),'Choose an exact private transaction nonce')
            release.require((args.apply_private and isinstance(args.expected_preflight,str) and release.SHA.fullmatch(args.expected_preflight))
                            or (not args.apply_private and args.expected_preflight is None),'A fresh apply requires its expected preflight digest')
            release.require(not args.resume or args.rollback_private or args.restore_public,
                            'Use --resume only with --rollback-private or --restore-public')
            after,uid=root_context(args.from_release)
            if private:
                mode='apply' if args.apply_private else 'resume' if args.resume_private else 'restore' if args.restore_public else 'rollback'
                value=run_action(args.from_release,after,uid,args.backup_release,mode,args.nonce,
                    expected_preflight=args.expected_preflight,resume_action=args.resume)
            else:value=preflight(args.from_release,after,uid,args.backup_release) if args.preflight else inspection(args.from_release,after,uid)
        print(json.dumps(value,indent=2));return 0
    except (OSError,ValueError,subprocess.SubprocessError) as error:
        print('Installed host upgrade: '+str(error)[:1000],file=sys.stderr);return 2


if __name__=='__main__':raise SystemExit(main())
