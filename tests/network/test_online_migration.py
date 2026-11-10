"""Coordinator contracts, not evidence of an in-game host handoff."""
import pytest

from online_migration import ArcadeMigration, MigrationError, SCHEMA, MAX_CHECKPOINT_BYTES


def room(*peers):
    now = [100.]
    migration = ArcadeMigration(clock=lambda: now[0])
    for peer in peers:
        migration.join(peer, schema=SCHEMA)
    digest = migration.publish(1, 1, 1, 60, b'validated native checkpoint fixture')
    for peer in peers:
        migration.acknowledge(peer, 1, 1, digest)
    return migration, now, digest


def test_graceful_handoff_requires_native_readiness_from_all_survivors():
    migration, now, digest = room(2, 3)
    migration.leave(1, 1)
    assert migration.status() == dict(match_id=migration.match_id, phase='preparing', epoch=2, host_peer=None,
        candidate=2, reason='', checkpoint_sequence=1, checkpoint_digest=digest, members=[2, 3])
    assert not migration.authorized_host(1, 1)
    assert not migration.authorized_host(2, 2)
    migration.heartbeat(2, 2)
    migration.heartbeat(3, 2)
    assert migration.phase == 'preparing'  # Bridge heartbeats do not resume the engine.
    checkpoint = migration.checkpoint_for(3, 2)
    assert checkpoint.digest == digest and checkpoint.tick == 60
    assert checkpoint.payload == b'validated native checkpoint fixture'
    migration.ready(2, 2, digest)
    assert migration.host_peer is None
    migration.ready(3, 2, digest)
    assert migration.authorized_host(2, 2)
    assert migration.phase == 'active' and set(migration.members) == {2, 3}
    assert not migration.authorized_host(1, 2)
    with pytest.raises(MigrationError, match='current member'):
        migration.heartbeat(1, 2)
    with pytest.raises(MigrationError, match='reused'):
        migration.join(1, schema=SCHEMA)


def test_abrupt_host_loss_uses_a_recent_acknowledged_checkpoint():
    migration, now, digest = room(2, 3)
    now[0] += 4.1
    migration.heartbeat(2, 1)
    migration.heartbeat(3, 1)
    assert not migration.authorized_host(1, 1)
    migration.poll()
    assert migration.phase == 'preparing' and migration.candidate == 2
    migration.ready(2, 2, digest)
    migration.ready(3, 2, digest)
    assert migration.authorized_host(2, 2)


def test_latest_available_checkpoint_wins_over_peer_number():
    migration, now, old = room(2, 3)
    latest = migration.publish(1, 1, 2, 70, b'newer engine state')
    migration.acknowledge(3, 1, 2, latest)
    migration.leave(1, 1)
    assert migration.candidate == 3 and migration.selected.sequence == 2
    with pytest.raises(MigrationError, match='does not match'):
        migration.ready(2, 2, old)
    migration.ready(2, 2, latest)
    migration.ready(3, 2, latest)
    assert migration.host_peer == 3


def test_newer_unacknowledged_checkpoint_does_not_replace_a_valid_backup():
    migration, now, digest = room(2, 3)
    migration.publish(1, 1, 2, 70, b'not received by any engine')
    migration.leave(1, 1)
    assert migration.selected.sequence == 1 and migration.selected.digest == digest
    migration.ready(2, 2, digest)
    migration.ready(3, 2, digest)
    assert [entry.sequence for entry in migration.checkpoints] == [1]


def test_failed_candidate_is_fenced_and_a_different_survivor_is_selected():
    migration, now, digest = room(2, 3, 4)
    migration.leave(1, 1)
    migration.ready(3, 2, digest)
    now[0] += 4
    for peer in (2, 3, 4):
        migration.heartbeat(peer, 2)
    migration.poll()  # Candidate2 is connected but never applied the checkpoint.
    assert migration.candidate == 3 and migration.epoch == 3 and 2 in migration.retired
    with pytest.raises(MigrationError, match='epoch'):
        migration.ready(2, 2, digest)
    migration.ready(3, 3, digest)
    migration.ready(4, 3, digest)
    assert migration.authorized_host(3, 3)


def test_candidate_disconnect_reselects_without_double_host_authority():
    migration, now, digest = room(2, 3)
    migration.leave(1, 1)
    migration.leave(2, 2)
    assert migration.candidate == 3 and migration.epoch == 3
    assert not any(migration.authorized_host(peer, epoch)
                   for peer in (1, 2, 3) for epoch in (1, 2, 3))
    migration.ready(3, 3, digest)
    assert migration.authorized_host(3, 3)


def test_slow_nonhost_is_excluded_before_ready_successor_resumes():
    migration, now, digest = room(2, 3)
    migration.leave(1, 1)
    migration.ready(2, 2, digest)
    now[0] += 4
    migration.heartbeat(2, 2)
    migration.heartbeat(3, 2)
    migration.poll()
    assert migration.host_peer == 2 and migration.retired == {1, 3}
    with pytest.raises(MigrationError, match='current member'):
        migration.ready(3, 2, digest)


def test_expired_ready_member_cannot_make_a_quorum_without_polling():
    migration, now, digest = room(2, 3)
    now[0] += 3
    migration.heartbeat(1, 1)
    migration.leave(1, 1)
    migration.ready(3, 2, digest)
    now[0] += 2
    migration.heartbeat(2, 2)
    migration.ready(2, 2, digest)
    assert migration.phase == 'preparing'
    migration.poll()
    assert migration.host_peer == 2 and 3 in migration.retired


def test_repeated_handoffs_keep_stable_ids_and_reject_old_owner_packets():
    migration, now, digest = room(2, 3, 4)
    migration.leave(1, 1)
    for peer in (2, 3, 4):
        migration.ready(peer, 2, digest)
    next_digest = migration.publish(2, 2, 2, 100, b'new host authoritative state')
    for peer in (3, 4):
        migration.acknowledge(peer, 2, 2, next_digest)
    migration.leave(2, 2)
    for peer in (3, 4):
        migration.ready(peer, 3, next_digest)
    assert migration.authorized_host(3, 3)
    for old_peer, old_epoch in ((1, 1), (2, 2)):
        with pytest.raises(MigrationError, match='epoch'):
            migration.publish(old_peer, old_epoch, 3, 110, b'stale old host')
    assert set(migration.members) == {3, 4}


@pytest.mark.parametrize('case', ['absent', 'stale', 'ineligible', 'unacknowledged'])
def test_unrecoverable_match_closes_without_pretending_to_migrate(case):
    now = [0.]
    migration = ArcadeMigration(clock=lambda: now[0])
    migration.join(2, schema=SCHEMA, can_host=case != 'ineligible')
    if case != 'absent':
        digest = migration.publish(1, 1, 1, 60, b'engine backup')
        if case != 'unacknowledged':
            migration.acknowledge(2, 1, 1, digest)
    if case == 'stale':
        now[0] = 9
        migration.heartbeat(1, 1)
        migration.heartbeat(2, 1)
    migration.leave(1, 1)
    assert migration.phase == 'closed' and migration.host_peer is None
    assert migration.reason and not migration.checkpoints


def test_bounded_checkpoints_and_exact_engine_acknowledgements():
    migration, now, first = room(2)
    with pytest.raises(MigrationError, match='unknown'):
        migration.acknowledge(2, 1, 1, '0'*64)
    for payload in (b'', b'x'*(MAX_CHECKPOINT_BYTES+1), bytearray(b'not immutable')):
        with pytest.raises(MigrationError, match='payload'):
            migration.publish(1, 1, 2, 70, payload)
    for sequence, tick in ((1, 70), (0, 70), (2, 59), (True, 70), (2, -1)):
        with pytest.raises(MigrationError):
            migration.publish(1, 1, sequence, tick, b'checkpoint')
    second = migration.publish(1, 1, 2, 70, b'checkpoint')
    third = migration.publish(1, 1, 3, 80, b'checkpoint')
    assert first != second != third
    assert len(migration.checkpoints) == 2
    assert first not in migration.members[2].acknowledged
    with pytest.raises(MigrationError, match='unknown'):
        migration.acknowledge(2, 1, 1, first)
    with pytest.raises(MigrationError, match='active game host'):
        migration.publish(2, 1, 4, 90, b'guest cannot replace authoritative state')


def test_story_legacy_clients_and_mid_handoff_joins_are_rejected():
    with pytest.raises(MigrationError, match='Only Arcade'):
        ArcadeMigration(mode='story')
    migration, now, digest = room(2)
    with pytest.raises(MigrationError, match='Native Arcade'):
        migration.join(3, schema='bridge-only')
    migration.leave(1, 1)
    with pytest.raises(MigrationError, match='during host migration'):
        migration.join(3, schema=SCHEMA)
    with pytest.raises(MigrationError, match='epoch'):
        migration.ready(2, 1, digest)


def test_all_players_lost_clears_snapshots_and_closes_once():
    migration, now, digest = room(2)
    now[0] += 5
    migration.poll()
    epoch = migration.epoch
    assert migration.phase == 'closed' and not migration.checkpoints
    migration.poll()
    assert migration.epoch == epoch and not migration.authorized_host(1, 1)


def test_checkpoint_acknowledgements_cannot_cross_match_identities():
    first = ArcadeMigration(match_id='a'*32)
    second = ArcadeMigration(match_id='b'*32)
    first.join(2, schema=SCHEMA)
    second.join(2, schema=SCHEMA)
    digest = first.publish(1, 1, 1, 60, b'identical payload')
    other_digest = second.publish(1, 1, 1, 60, b'identical payload')
    assert digest != other_digest
    with pytest.raises(MigrationError, match='unknown'):
        second.acknowledge(2, 1, 1, digest)


def test_admission_bounds_and_deadline_validation():
    for peer in (0, 255, True, -1, [], '2'):
        with pytest.raises(MigrationError, match='peer ID'):
            ArcadeMigration(host_peer=peer)
    for options in (dict(mode='story'), dict(lease_seconds=-1), dict(prepare_seconds=float('inf')),
                    dict(checkpoint_seconds=2), dict(match_id='invalid')):
        with pytest.raises(MigrationError):
            ArcadeMigration(**options)
    migration = ArcadeMigration()
    for peer in range(2, 17):
        migration.join(peer, schema=SCHEMA)
    with pytest.raises(MigrationError, match='full'):
        migration.join(17, schema=SCHEMA)
