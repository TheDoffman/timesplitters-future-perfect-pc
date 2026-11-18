"""Read stopped monitor/layout generations after reboot; no file exchange."""
import os

import mod_host_cutover as cutover
import mod_host_cutover_journal as storage
import mod_host_reboot_observation as observations
import mod_host_routing as routes
import mod_host_upgrade_activation as activation
import mod_host_upgrade_admission as admission
import mod_host_upgrade_context as contexts
import mod_host_upgrade_layout as layouts
import mod_host_upgrade_shutdown as shutdown
from mod_host_migration import boot_identity
from mod_package import ModError,read_json,require


def read(journal,fresh):
    base=journal.base;state=journal.state();reboot=state['reboot']
    require(os.geteuid()==cutover.ROOT_UID and reboot is not None and reboot['quiescent']
            and journal.observation_boot==reboot['boot_id']==boot_identity() and state['layout_sha256'] is not None,
            'Read prepared reboot layout only after current quiescence')
    metadata=contexts.read_reboot_context(journal,fresh)
    require(metadata['context_sha256']==reboot['context_sha256'],'Reboot layout changes original context')
    choices=observations.policies(fresh['entries'],fresh['candidate_entries'])
    def guard():
        require(journal.state()['reboot']['boot_id']==boot_identity(),'Boot changed during reboot layout observation')
        require(storage.wire(observations.files(choices))==storage.wire(reboot['observation']['files']),
                'Protected reboot files changed after quiescence')
        require(all(item is None for item in observations.loaded(choices,reboot['observation']['files'],fresh['command'])['jobs'].values()),
                'Workshop jobs reappeared during reboot layout observation')
        code,output=fresh['command'](['/bin/ps','-axo','pid=,ppid=,pgid=,uid='])
        require(code==0 and cutover.processes_gone(reboot['observation']['tracked'],cutover.process_rows(output),base['uid']),
                'Reboot descendants or service processes reappeared')
        route=routes.snapshot(fresh['operator']('status'),base['routing']['origin'])
        require(route['phase']!='public' and route['unrelated_sha256']==base['routing']['unrelated_sha256'],
                'Public or unrelated routing changed during reboot layout observation')
    guard()
    layout=layouts.Layout(journal,cutover.ROOT/'releases'/base['before'],cutover.ROOT/'releases'/base['after'],
        fresh['entries'],fresh['candidate_entries'],observation=True)
    activation.private_layout_storage(layout.folder);layout.reopen(state['layout_sha256'])
    manifest=read_json(layout.manifest)
    require(manifest['backup']==layout._backup(),'Preserved backup identities changed after reboot')
    candidates=dict(before=manifest['monitor'],after=manifest['after_monitor'])
    for generation,sha in state.get('monitor_checkpoints',{}).items():
        candidates[generation]=shutdown.read_checkpoint(layout,generation,sha)
    found={};inventories={}
    for name,path in (('active',cutover.ROOT/'monitor'),('saved',layout.saved),('staged',layout.staged)):
        try:inventory=layouts.snapshot(path)
        except FileNotFoundError:found[name]=None;continue
        if 'upgrade-quiescence.json' in inventory:
            admission.checked_existing(journal,cutover.protected_bytes(path/'upgrade-quiescence.json',0o644,8192)[0])
        matches=[]
        for generation,baseline in candidates.items():
            try:
                if name=='active':
                    entries=fresh['entries' if generation=='before' else 'candidate_entries']
                    current=shutdown.checked_monitor_history(journal,path,baseline,entries,generation)
                    require(current==inventory,'Active monitor changed while observing')
                else:
                    stable=lambda value:{key:item for key,item in value.items() if key!='upgrade-quiescence.json'}
                    require(stable(inventory)==stable(baseline),'Archived monitor inventory changed')
                for relative,expected in (layout.before if generation=='before' else layout.after).items():
                    data,_=cutover.protected_bytes(path/relative,0o600,32768)
                    require(read_json(data)==read_json(expected) if relative.endswith('.json') else data==expected,
                            'Protected reboot monitor policy differs from its factory')
                original_proof=generation=='before' and 'upgrade-quiescence.json' in manifest['monitor']
                if original_proof and 'upgrade-quiescence.json' not in inventory:
                    require(activation.retired_original_proof(journal,layout.folder,'before'),'Original migration proof disappeared')
                matches.append(generation)
            except ModError:continue
        require(len(matches)==1,'Preserving unknown or ambiguous reboot monitor evidence')
        found[name]=matches[0];inventories[name]=inventory
        require(layouts.snapshot(path)==inventory,'Monitor evidence changed while observing')
    require(sorted(side for side in found.values() if side is not None)==['after','before']
            and found['saved'] in (None,'before') and found['staged'] in (None,'after'),
            'Reboot layout has duplicate or missing protected generations')
    guard()
    require(cutover.protected_bytes(layout.folder/'manifest.json',0o600,65536)[0]==layout.manifest,
            'Original layout manifest changed during reboot observation')
    result=dict(schema=1,phase='reboot-layout-observed',nonce=base['nonce'],boot_id=reboot['boot_id'],original_boot=base['boot_id'],
        context_sha256=reboot['context_sha256'],layout_sha256=state['layout_sha256'],locations=found,inventories=inventories,
        services_started=False,files_changed=False,operational_admission=False)
    require(len(storage.wire(result))<=65536,'Reboot layout observation exceeds budget')
    return result
