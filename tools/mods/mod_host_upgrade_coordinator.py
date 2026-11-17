"""Compose verified upgrade phases under the caller's already-held Root lease.

No CLI imports this until installed recovery is integrated. Closure verification
and journal creation belong to the immutable entry point, before calling here.
Failures preserve the pending history and suspended ingress for explicit recovery.
"""
import os

import mod_host_cutover as cutover
import mod_host_migration as migration
import mod_host_routing as routes
import mod_host_upgrade_admission as admission
import mod_host_upgrade_activation as activation
import mod_host_upgrade_layout as layouts
from mod_package import require


def checked_context(journal,context):
    base=journal.base;report=context['report']
    old,current=context['old'],context['new']
    require(os.geteuid()==cutover.ROOT_UID and base['boot_id']==migration.boot_identity()
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
            and state['layout_complete'] in (None,'after') and state['config_pending'] in (None,'apply')
            and (state['layout_pending'] is None or state['layout_pending'].startswith('apply-'))
            and state['shutdown'] is None and state['activation_generation'] in (None,'after'),
            'Forward upgrade cannot cross restoration, reversal or shutdown recovery')
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
