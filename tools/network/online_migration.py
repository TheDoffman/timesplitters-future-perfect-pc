"""Arcade migration coordinator; not yet connected to the game or live relay.

The directory must authenticate callers and serialize these methods under its
room lock. Native clients must validate/capture/apply a portable checkpoint before
acknowledging it. Transport connectivity alone never proves game readiness.
"""
from dataclasses import dataclass, field
import hashlib
import math
import re
import secrets
import time

SCHEMA = 'tsfp-arcade-checkpoint-v1'
MAX_CHECKPOINT_BYTES = 256 * 1024
MAX_MEMBERS = 16


class MigrationError(ValueError):
    pass


@dataclass
class Member:
    peer: int
    can_host: bool
    seen: float
    acknowledged: set = field(default_factory=set)


@dataclass(frozen=True)
class Checkpoint:
    sequence: int
    tick: int
    epoch: int
    created: float
    digest: str
    payload: bytes


class ArcadeMigration:
    """One elected host, stable player IDs, bounded backups and fenced epochs.

    No game addresses or profile data belong in this class. It deliberately does
    not promote an engine role, reroute existing packets or infer readiness from
    a companion heartbeat. Those require a separately verified native adapter.
    """
    def __init__(self, host_peer=1, *, mode='arcade', match_id=None, clock=time.monotonic,
                 lease_seconds=4., prepare_seconds=4., checkpoint_seconds=8.):
        if mode != 'arcade':
            raise MigrationError('Only Arcade supports this migration protocol')
        self._peer_id(host_peer)
        if not all(isinstance(value, (int, float)) and math.isfinite(value) and value > 0
                   for value in (lease_seconds, prepare_seconds, checkpoint_seconds)):
            raise MigrationError('Migration deadlines must be positive and finite')
        if checkpoint_seconds <= lease_seconds:
            raise MigrationError('Checkpoint lifetime must exceed the host-loss deadline')
        self.match_id = match_id if match_id is not None else secrets.token_hex(16)
        if not isinstance(self.match_id, str) or not re.fullmatch('[0-9a-f]{32}', self.match_id):
            raise MigrationError('Match identity must be 16 lowercase hexadecimal bytes')
        self.clock = clock
        self.lease_seconds = lease_seconds
        self.prepare_seconds = prepare_seconds
        self.checkpoint_seconds = checkpoint_seconds
        self.epoch = 1
        self.host_peer = host_peer
        self.phase = 'active'
        self.reason = ''
        self.members = {host_peer: Member(host_peer, True, clock())}
        self.retired = set()
        self.checkpoints = []
        self.candidate = None
        self.selected = None
        self.ready_members = set()
        self.deadline = None

    @staticmethod
    def _peer_id(peer):
        if type(peer) is not int or not 1 <= peer <= 254:
            raise MigrationError('Invalid stable peer ID')

    @staticmethod
    def _counter(value, name):
        if type(value) is not int or not 0 <= value <= 0xffffffff:
            raise MigrationError('Invalid '+name)

    def _member(self, peer, epoch):
        self._peer_id(peer)
        if epoch != self.epoch or type(epoch) is not int:
            raise MigrationError('Stale authority epoch')
        if self.phase == 'closed':
            raise MigrationError('Match is closed')
        if peer not in self.members or type(peer) is not int:
            raise MigrationError('Peer is not a current member')
        return self.members[peer]

    def join(self, peer, *, schema, can_host=True):
        self._peer_id(peer)
        if schema != SCHEMA or type(can_host) is not bool:
            raise MigrationError('Native Arcade checkpoint support is required')
        if self.phase != 'active':
            raise MigrationError('Cannot join during host migration')
        if peer in self.members or peer in self.retired:
            raise MigrationError('Peer ID cannot be reused')
        if len(self.members) >= MAX_MEMBERS:
            raise MigrationError('Arcade room is full')
        self.members[peer] = Member(peer, can_host, self.clock())

    def heartbeat(self, peer, epoch):
        self._member(peer, epoch).seen = self.clock()

    def publish(self, peer, epoch, sequence, tick, payload):
        self._member(peer, epoch)
        if not self.authorized_host(peer, epoch):
            raise MigrationError('Only the active game host can publish checkpoints')
        self._counter(sequence, 'checkpoint sequence')
        self._counter(tick, 'match tick')
        if not isinstance(payload, bytes) or not 0 < len(payload) <= MAX_CHECKPOINT_BYTES:
            raise MigrationError('Checkpoint payload is empty or exceeds the size limit')
        if self.checkpoints and (sequence <= self.checkpoints[-1].sequence or tick < self.checkpoints[-1].tick):
            raise MigrationError('Checkpoint sequence/tick must not go backwards')
        # Include the metadata in the digest so an ACK cannot be transplanted to
        # another sequence/epoch carrying identical state bytes.
        header = f'{SCHEMA}:{self.match_id}:{epoch}:{sequence}:{tick}:'.encode('ascii')
        checkpoint = Checkpoint(sequence, tick, epoch, self.clock(),
                                hashlib.sha256(header+payload).hexdigest(), payload)
        self.checkpoints = [*self.checkpoints[-1:], checkpoint]
        retained = {entry.digest for entry in self.checkpoints}
        for member in self.members.values():
            member.acknowledged.intersection_update(retained)
        self.members[peer].seen = self.clock()
        return checkpoint.digest

    def acknowledge(self, peer, epoch, sequence, digest):
        member = self._member(peer, epoch)
        self._counter(sequence, 'checkpoint sequence')
        checkpoint = next((entry for entry in self.checkpoints
                           if entry.sequence == sequence and entry.digest == digest), None)
        if checkpoint is None or self.clock()-checkpoint.created > self.checkpoint_seconds:
            raise MigrationError('Checkpoint is unknown or expired')
        # This is an engine acknowledgement, not a receipt of downloaded bytes.
        member.acknowledged.add(digest)

    def authorized_host(self, peer, epoch):
        return (type(peer) is int and type(epoch) is int and self.phase == 'active' and
                peer == self.host_peer and epoch == self.epoch and peer in self.members and
                self.clock()-self.members[peer].seen <= self.lease_seconds)

    def _retire(self, peer):
        self.members.pop(peer, None)
        self.retired.add(peer)
        self.ready_members.discard(peer)

    def _close(self, reason):
        self.epoch += 1
        self.phase = 'closed'
        self.reason = reason
        self.host_peer = self.candidate = self.selected = self.deadline = None
        self.ready_members.clear()
        self.checkpoints.clear()
        for member in self.members.values():
            member.acknowledged.clear()

    def _prepare(self):
        # Changing epoch fences the old host even while the successor is still
        # loading. A production relay must enforce this before forwarding.
        self.epoch += 1
        self.host_peer = None
        self.ready_members.clear()
        now = self.clock()
        choices = [(entry.sequence, -member.peer, member.peer, entry)
                   for member in self.members.values() if member.can_host
                   and now-member.seen <= self.lease_seconds
                   for entry in self.checkpoints if entry.digest in member.acknowledged
                   and now-entry.created <= self.checkpoint_seconds]
        if not choices:
            self._close('No connected replacement has a fresh native checkpoint')
            return
        _, _, self.candidate, self.selected = max(choices, key=lambda choice: choice[:2])
        self.phase = 'preparing'
        self.deadline = now+self.prepare_seconds

    def leave(self, peer, epoch):
        self._member(peer, epoch)
        was_host = peer == self.host_peer
        was_candidate = peer == self.candidate
        self._retire(peer)
        if not self.members:
            self._close('No players remain')
        elif was_host or was_candidate:
            self._prepare()
        elif self.phase == 'preparing':
            self._commit_if_ready()

    def checkpoint_for(self, peer, epoch):
        self._member(peer, epoch)
        if self.phase != 'preparing' or self.selected is None:
            raise MigrationError('No migration checkpoint is selected')
        return self.selected

    def ready(self, peer, epoch, digest):
        member = self._member(peer, epoch)
        if self.phase != 'preparing' or self.selected is None or digest != self.selected.digest:
            raise MigrationError('Native readiness does not match this handoff')
        if self.clock() >= self.deadline or self.clock()-member.seen > self.lease_seconds:
            raise MigrationError('Readiness arrived after its deadline')
        self.ready_members.add(peer)
        member.acknowledged.add(digest)
        self._commit_if_ready()

    def _commit_if_ready(self):
        if (self.candidate in self.ready_members and self.ready_members == set(self.members) and
            all(self.clock()-member.seen <= self.lease_seconds for member in self.members.values())):
            self.host_peer = self.candidate
            self.candidate = None
            self.phase = 'active'
            self.deadline = None
            self.ready_members.clear()
            # The newly restored game resumes from the selected checkpoint. Do
            # not later fail over to an unselected, divergent newer snapshot.
            self.checkpoints = [self.selected]
            for member in self.members.values():
                member.acknowledged.intersection_update({self.selected.digest})
            self.selected = None

    def poll(self):
        if self.phase == 'closed':
            return
        now = self.clock()
        lost = [peer for peer, member in self.members.items()
                if now-member.seen > self.lease_seconds]
        host_lost = self.host_peer in lost
        candidate_lost = self.candidate in lost
        for peer in lost:
            self._retire(peer)
        if not self.members:
            self._close('No players remain')
        elif host_lost or candidate_lost:
            self._prepare()
        elif self.phase == 'preparing' and now >= self.deadline:
            if self.candidate not in self.ready_members:
                self._retire(self.candidate)
                self._prepare()
            else:
                # A responsive successor must not wait forever for a stalled
                # native client. Excluded peers are fenced before resuming.
                for peer in set(self.members)-self.ready_members:
                    self._retire(peer)
                self._commit_if_ready()
        elif self.phase == 'preparing':
            self._commit_if_ready()

    def status(self):
        return dict(match_id=self.match_id, phase=self.phase, epoch=self.epoch, host_peer=self.host_peer,
                    candidate=self.candidate, reason=self.reason,
                    checkpoint_sequence=self.selected.sequence if self.selected else None,
                    checkpoint_digest=self.selected.digest if self.selected else None,
                    members=sorted(self.members))
