"""Compose verified upgrade phases under the caller's already-held Root lease.

Closure verification and journal/context admission belong to the immutable entry
point, before calling here. Private and public phases are explicitly selected.
Failures preserve the pending history and suspended ingress for explicit recovery.
"""
import os

import mod_host_cutover as cutover
import mod_host_cutover_journal as history
import mod_host_migration as migration
import mod_host_routing as routes
import mod_host_upgrade_admission as admission
import mod_host_upgrade_activation as activation
import mod_host_upgrade_layout as layouts
import mod_host_upgrade_shutdown as shutdown
import mod_host_upgrade_process as process
from mod_package import read_json,require


def checked_context(journal,context):
    base=journal.base;report=context['report']
    old,current=context['old'],context['new']
    require(os.geteuid()==cutover.ROOT_UID and history.operational_boot(journal)==migration.boot_identity()
            and all(report[key]==base[key] for key in ('before','after','uid','backup_identity',
                'backup_base_release','plan_sha256','preflight_sha256'))
            and old['identity']==base['before'] and current['identity']==base['after']
            and old['uid']==current['uid']==base['uid']
            and context['backup_runtime']['identity']==base['backup_base_release']
            and context['backup_runtime']['uid']==base['uid']
            and context['routing']==base['routing']
            and context['observed']['observation']['tracked']==base['tracked']
            and context['plan']['sha256']==base['plan_sha256']
            and context['profile']['origin']==base['routing']['origin']
            and context['private_baseline']['release']==base['before']
            and all(context['profile'][key]==context['private_baseline'][key]
                    for key in ('origin','trusted_root_sha256')),
            'Upgrade context differs from the held Root transaction')
    cutover.validate_entries(context['entries']);cutover.validate_entries(context['candidate_entries'])
    binding=context['backup_binding']
    require(binding['uid']==binding['gid']==base['uid'] and binding['base_release']==base['backup_base_release']
            and binding['python']==str(context['backup_runtime']['python']),
            'Upgrade context changes the preserved backup binding')
    return context


def apply_private(journal,context,*,resume=False):
    """Suspend ingress, capture, migrate/exchange, start and admit ten jobs.

    Returned admission still needs separate fresh routing restoration and public
    HTTPS acceptance. Explicit same-boot resume re-observes the journal's known
    effects; it never implies that a pending OS operation failed or repeats a
    pending bootstrap/restart. Rollback/reboot use separate recovery paths.
    """
    require(type(resume) is bool,'Choose explicit upgrade recovery')
    checked_context(journal,context)
    state=journal.state();base=journal.base
    require(not state['settled'] and not state['routing_restored'] and state['routing_pending'] is not True
            and state['layout_complete'] in (None,'before','after') and state['config_pending'] in (None,'apply')
            and (state['layout_pending'] is None or state['layout_pending'].startswith('apply-'))
            and state['shutdown'] is None and state['activation_generation'] in (None,'after'),
            'Forward upgrade cannot cross restoration, reversal or shutdown recovery')
    require(not any(event['kind'] in ('migration-intent','layout-intent','layout-complete')
            and isinstance(event['label'],str) and event['label'].startswith('rollback')
            for event in read_json(journal.current)['events']),
            'A recorded reversal requires its own explicit recovery path')
    require(resume or (not state['routing_suspended'] and state['routing_pending'] is None
            and not state['stopped'] and state['pending'] is None and state['layout_sha256'] is None),
            'An existing upgrade requires explicit recovery')
    if state['private_baseline'] is None:
        journal.record('baseline-private-verified',data=context['private_baseline'])
    else:require(state['private_baseline']==context['private_baseline'],'Original private baseline changed')
    invoke,command=context['operator'],context['command']
    if not journal.state()['routing_suspended'] or journal.state()['routing_pending'] is not None:
        routes.transition(base['routing'],False,invoke,journal.routing_event,resume=resume)
    journal.require_suspended(invoke)
    old_entries,new_entries=context['entries'],context['candidate_entries']
    exchange=layouts.Layout(journal,context['old']['root'],context['new']['root'],old_entries,new_entries)
    state=journal.state()
    if state['layout_sha256'] is not None:exchange.reopen(state['layout_sha256'])
    if state['layout_complete']!='after' or state['config']!='after' or state['config_pending'] is not None:
        # Migration authority is needed only while its effect is unresolved.
        # Once configuration is durably after, partial boot-file exchange can
        # finish without issuing a new proof through a deliberately mixed tree.
        if state['config']=='before' or state['config_pending']=='apply':
            cutover.quiesce(old_entries,base['uid'],journal,command,routing=invoke)
            admission.capture_and_admit(journal,old_entries,context['backup_runtime'],context['new'],
                context['profile'],context['plan'],invoke,command)
        gate=lambda:layouts.quiescence_gate(journal,old_entries,command,invoke)
        gate()
        if exchange.manifest is None:
            fingerprint=exchange.prepare()
            gate();journal.record('layout-prepared',data=fingerprint)
        layouts.configuration_and_exchange(journal,context['new'],exchange,'apply',gate,resume=resume)
    activation.start_private_services(journal,context['new'],exchange,new_entries,command,invoke,resume=resume)
    result=activation.admit_private_services(journal,context['new'],exchange,new_entries,context['profile'],
        command,invoke,resume=resume)
    journal.require_suspended(invoke)
    return result


def rollback_private(journal,context,*,resume=False):
    """Reverse a completed candidate layout and admit the preserved old services.

    Explicit resume resolves pending forward migration/file exchange in its
    original direction before reversal, without starting candidate jobs. This
    phase also accepts pending candidate startup/restart/shutdown and
    pending reverse effects. It never restores repository data from a backup:
    the old service must pass readiness against the retained current data.
    """
    require(type(resume) is bool,'Choose explicit rollback recovery')
    checked_context(journal,context)
    state=journal.state();base=journal.base
    reboot=state['reboot']
    unstaged=(state['layout_sha256'] is None and reboot is not None and reboot['admitted'] is not None
        and state['config']=='before' and state['config_pending'] is None and state['layout_pending'] is None
        and state['layout_complete'] is None and state['monitor']=='before' and state['quiescent']
        and all(value=='before' for value in state['boots'].values()) and not state['started'] and state['start_pending'] is None)
    require(not state['settled'] and not state['routing_restored'] and state['routing_pending'] is None
            and (state['layout_sha256'] is not None or unstaged) and state['config_pending'] in (None,'apply','rollback')
            and (state['layout_pending'] is None or state['layout_pending'].startswith(('apply-','rollback-')))
            and state['private_baseline']==context['private_baseline'],
            'Resolve forward or routing recovery before reversing a prepared upgrade')
    history=read_json(journal.current)['events']
    reversing=(state['shutdown'] is not None or state['activation_generation']=='before'
        or 'before' in state['retired_proofs']
        or (state['retire_pending'] is not None and state['retire_pending']['generation']=='before')) or any(
        event['kind'] in ('migration-intent','layout-intent','layout-complete')
        and isinstance(event['label'],str) and event['label'].startswith('rollback') for event in history)
    require(resume or not reversing,'An existing reversal requires explicit recovery')
    forward_pending=(state['config_pending']=='apply'
        or (state['layout_pending'] is not None and state['layout_pending'].startswith('apply-'))
        or (not reversing and state['config']=='after' and state['layout_complete']!='after'))
    require(not forward_pending or (resume and not reversing and state['quiescent'] and not state['started']
            and state['start_pending'] is None and state['shutdown'] is None and state['retire_pending'] is None),
            'Pending forward effects require explicit quiescent resolution before rollback')
    invoke,command=context['operator'],context['command']
    journal.require_suspended(invoke)
    old_entries,new_entries=context['entries'],context['candidate_entries']
    exchange=layouts.Layout(journal,context['old']['root'],context['new']['root'],old_entries,new_entries)
    gate=lambda:layouts.quiescence_gate(journal,old_entries,command,invoke)
    if unstaged:
        # No candidate configuration or boot policy was applied. Bind the old
        # tree to a protected manifest, then admit old startup directly; do not
        # migrate forward merely to permit an early cancellation.
        gate();fingerprint=exchange.prepare();gate();journal.record('layout-prepared',data=fingerprint)
        state=journal.state()
    else:exchange.reopen(state['layout_sha256'])
    if forward_pending:
        gate()
        if state['config']=='before' or state['config_pending']=='apply':
            admission.capture_and_admit(journal,old_entries,context['backup_runtime'],context['new'],
                context['profile'],context['plan'],invoke,command)
        layouts.configuration_and_exchange(journal,context['new'],exchange,'apply',gate,resume=True)
        state=journal.state()
        require(state['config']=='after' and state['layout_complete']=='after' and state['layout_pending'] is None
                and state['config_pending'] is None,'Forward effects remain unresolved; preserving them before rollback')
    if state['config']=='after' and state['layout_complete']=='after':
        if state['retire_pending'] is not None:
            require(resume,'Pending proof retirement requires explicit recovery')
            activation.retire_proof(journal,exchange,gate,resume=True)
        state=journal.state()
        if state['started'] or state['start_pending'] is not None or state['shutdown'] is not None:
            if not state['shutdown_complete']:
                shutdown.stop_private_services(journal,exchange,new_entries,command,invoke,resume=resume)
            if 'after' not in journal.state()['monitor_checkpoints']:
                shutdown.checkpoint_monitor(journal,exchange,new_entries,gate,resume=resume)
            else:exchange._locations()
    state=journal.state()
    require((state['quiescent'] and not state['started'] and state['start_pending'] is None)
            or (state['config']=='before' and state['layout_complete']=='before'
                and state['activation_generation']=='before' and resume),
            'Rollback requires observed shutdown or known old-generation startup recovery')
    if state['config']=='after':
        # Restore the complete old monitor/boot tree first. Its archived proof
        # can be expired. A fresh capture below publishes current authority in
        # that restored tree before the private rollback step consumes it.
        gate();exchange._locations()
        if state['layout_complete']!='before':
            result=exchange.exchange('rollback',journal.layout_event,gate,resume=resume,
                pending=state['layout_pending'])
            journal.record('layout-complete','rollback',result)
        admission.capture_and_admit(journal,old_entries,context['backup_runtime'],context['new'],
            context['profile'],context['plan'],invoke,command)
        layouts.configuration_and_exchange(journal,context['new'],exchange,'rollback',gate,resume=True)
    else:
        require(state['layout_complete']=='before' and state['config_pending'] is None,
                'Old private configuration lacks a complete restored layout')
    activation.start_private_services(journal,context['old'],exchange,old_entries,command,invoke,resume=resume)
    result=activation.admit_private_services(journal,context['old'],exchange,old_entries,context['profile'],
        command,invoke,resume=resume)
    journal.require_suspended(invoke)
    return result


def restore_public(journal,context,*,resume=False):
    """Refresh private admission, restore only8443 and prove public signed reads.

    A pending restoration is re-observed before any routing action. Its private
    evidence can be refreshed even after expiration, without restarting jobs or
    repeating a migration. Both complete forward and reverse layouts use the
    newly verified public checker, observing the selected live host identity.
    """
    require(type(resume) is bool,'Choose explicit public restoration recovery')
    checked_context(journal,context)
    state=journal.state();base=journal.base;generation=state['config']
    require(not state['settled'] and state['activation_ready'] and state['layout_sha256'] is not None
            and state['layout_complete']==state['monitor']==state['activation_generation']==generation
            and all(value==generation for value in state['boots'].values())
            and state['started']==history.start_order()
            and state['pending'] is None and state['start_pending'] is None and state['restart_pending'] is None
            and state['config_pending'] is None and state['layout_pending'] is None and state['retire_pending'] is None
            and state['shutdown'] is None and state['routing_pending'] is not False
            and (state['routing_suspended'] or state['routing_restored']),
            'Public restoration requires complete admitted private services')
    require(resume or (state['routing_pending'] is None and not state['routing_restored']),
            'Interrupted public restoration requires explicit recovery')
    invoke,command=context['operator'],context['command']
    current=context['old' if generation=='before' else 'new']
    entries=context['entries' if generation=='before' else 'candidate_entries']
    exchange=layouts.Layout(journal,context['old']['root'],context['new']['root'],context['entries'],context['candidate_entries'])
    exchange.reopen(state['layout_sha256'])
    files=cutover.job_files(entries)
    def guard(expected=None):
        checked_context(journal,context)
        activation.fixed_monitor_files(exchange,generation);cutover.job_files(entries,files)
        activation._observe_known(journal,entries,command)
        observed=routes.snapshot(invoke('status'),base['routing']['origin'])
        state=journal.state()
        require(observed['unrelated_sha256']==base['routing']['unrelated_sha256']
                and (observed['phase']!='public' or state['routing_pending'] is True or state['routing_restored'])
                and (expected is None or observed==expected),'Public restoration routing changed unexpectedly')
        return observed
    observed=guard()
    scanner=process.scanner_check(current,base['uid'])
    scanner_time=int(journal.clock());guard(observed)
    private=process.private_readiness(current,base['uid'],context['profile'],floor=journal.state()['catalog_floor'])
    private_time=int(journal.clock());guard(observed)
    journal.record('restoration-readiness-verified',generation,dict(routing=observed,scanner=scanner,private=private,
        scanner_checked_at=scanner_time,private_checked_at=private_time))
    state=journal.state()
    if not state['routing_restored'] or state['routing_pending'] is not None:
        routes.transition(base['routing'],True,invoke,journal.routing_event,resume=resume)
    guard(base['routing'])
    public=process.public_readiness(context['new'],base['uid'],context['profile'],
        floor=journal.state()['catalog_floor'],host_release=current['identity'])
    guard(base['routing'])
    journal.record('public-readiness-verified',generation,public)
    guard(base['routing'])
    journal.record('settled')
    return dict(phase='public-upgrade-settled',generation=generation,release=current['identity'],jobs=10,
        catalog_version=public['public']['catalog_version'],public_endpoint_verified=True,revocations_preserved=True)
