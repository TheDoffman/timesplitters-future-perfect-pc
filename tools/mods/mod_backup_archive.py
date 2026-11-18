"""Private snapshot framing and streaming encryption; no transport or scheduler.

Only verified snapshot inventory names are accepted. Recovery produces a private
snapshot, never a live repository; mod_backup.restore still applies its barrier.
"""
from contextlib import closing
import hashlib
import os
import shutil
import struct

import mod_backup as backup
import mod_backup_cipher as cipher
from mod_package import read_json, require
from mod_trust import encode

MAGIC = b'TSFPBA1\n'
MAX_MANIFEST = 1024**2


def header(manifest):
    total = backup.checked_manifest(manifest)
    wire = encode(manifest)
    require(0 < len(wire) <= MAX_MANIFEST, 'Snapshot manifest exceeds archive budget')
    return MAGIC + struct.pack('>I', len(wire)) + wire, total


def chunks(source, manifest):
    require(backup.backup_manifest(source)[1] == manifest, 'Snapshot inventory changed')
    yield header(manifest)[0]
    for name, expected in sorted(manifest['files'].items()):
        checksum = hashlib.sha256()
        with cipher.incoming(source / name, expected['size']) as (stream, size, stable):
            require(size == expected['size'], 'Snapshot file size changed')
            remaining = size
            while remaining:
                data = stream.read(min(cipher.CHUNK, remaining))
                require(data, 'Snapshot file shortened')
                checksum.update(data)
                remaining -= len(data)
                yield data
            require(stream.read(1) == b'' and checksum.hexdigest() == expected['sha256'],
                    'Snapshot file differs from verified inventory')
            stable()
    require(backup.backup_manifest(source)[1] == manifest, 'Snapshot inventory changed')


class Reader:
    """Adapt bounded chunks to exact-length reads without a whole-file buffer."""
    def __init__(self, source, manifest):
        self.iterator = chunks(source, manifest)
        self.pending = b''
        self.complete = False

    def read(self, length):
        require(type(length) is int and 0 < length <= cipher.CHUNK, 'Unbounded snapshot read refused')
        output = bytearray()
        while len(output) < length:
            if not self.pending:
                try:
                    self.pending = next(self.iterator)
                except StopIteration:
                    self.complete = True
                    break
            count = min(length - len(output), len(self.pending))
            output.extend(self.pending[:count])
            self.pending = self.pending[count:]
        return bytes(output)

    def stable(self):
        require(self.complete, 'Snapshot stream has not completed verification')

    def close(self):
        self.iterator.close()


def encrypt_snapshot(source, destination, recipient, service_origin, pin):
    verified = backup.verify_snapshot(source)
    source, manifest, total = backup.backup_manifest(source)
    require(cipher.digest((source / backup.MANIFEST).read_bytes()) == verified['manifest_sha256'],
            'Verified snapshot manifest changed')
    prefix, _ = header(manifest)
    size = len(prefix) + total
    require(size <= cipher.MAX_PLAIN, 'Snapshot archive exceeds encryption budget')
    checksum = hashlib.sha256()
    counted = 0
    with closing(Reader(source, manifest)) as stream:
        while data := stream.read(cipher.CHUNK):
            counted += len(data)
            checksum.update(data)
        stream.stable()
    require(counted == size, 'Snapshot archive size differs')
    with closing(Reader(source, manifest)) as stream:
        receipt = cipher.encrypt_stream(stream, destination, recipient, service_origin, pin,
                                       plain_bytes=size, plain_sha256=checksum.hexdigest(),
                                       verify_source=stream.stable)
    return dict(receipt, phase='backup-snapshot-encrypted', snapshot=verified)


def unpack_snapshot(source, destination, *, plain_sha256):
    """Unpack only a successfully decrypted archive with its private local digest.

    A SHA from the archive itself is insufficient. The caller must first use
    decrypt_file with the independently authenticated ciphertext receipt.
    """
    cipher.checked_sha(plain_sha256)
    destination = cipher.private_path(destination)
    with cipher.incoming(source, cipher.MAX_PLAIN) as (stream, size, stable):
        checksum = hashlib.sha256()
        remaining = size
        while remaining:
            data = stream.read(min(cipher.CHUNK, remaining))
            require(data, 'Snapshot archive shortened')
            checksum.update(data)
            remaining -= len(data)
        require(stream.read(1) == b'' and checksum.hexdigest() == plain_sha256,
                'Snapshot archive differs from decrypted receipt')
        stable()
        stream.seek(0)
        consumed = hashlib.sha256()

        def read(length):
            data = stream.read(length)
            consumed.update(data)
            require(len(data) == length, 'Snapshot archive truncated')
            return data

        require(read(len(MAGIC)) == MAGIC, 'Unknown snapshot archive magic')
        length = struct.unpack('>I', read(4))[0]
        require(0 < length <= MAX_MANIFEST, 'Snapshot manifest exceeds archive budget')
        wire = read(length)
        manifest = read_json(wire)
        prefix, total = header(manifest)
        require(prefix == MAGIC + struct.pack('>I', length) + wire and size == len(prefix) + total,
                'Noncanonical snapshot manifest or archive size')
        # Check space before creating any output. Only POSIX host recovery is
        # supported; Windows keeps ciphertext until recovery on Mac/Linux.
        backup.private_directory(destination.parent)
        require(shutil.disk_usage(destination.parent).free >= total + length + cipher.MIN_FREE,
                'Insufficient snapshot recovery headroom')
        output = backup.fresh_directory(destination)
        (output / 'blobs').mkdir(mode=0o700)
        (output / 'incoming').mkdir(mode=0o700)
        for name, expected in sorted(manifest['files'].items()):
            checksum = hashlib.sha256()
            with os.fdopen(os.open(output / name, os.O_CREAT | os.O_EXCL | os.O_WRONLY |
                                   getattr(os, 'O_NOFOLLOW', 0), 0o600), 'wb') as target:
                remaining = expected['size']
                while remaining:
                    data = read(min(cipher.CHUNK, remaining))
                    target.write(data)
                    checksum.update(data)
                    remaining -= len(data)
                require(checksum.hexdigest() == expected['sha256'], 'Snapshot member integrity check failed')
                target.flush()
                os.fsync(target.fileno())
        require(stream.read(1) == b'' and consumed.hexdigest() == plain_sha256,
                'Snapshot archive changed after receipt verification')
        stable()
        # Inventory is written last. A partial extraction lacks a valid snapshot
        # manifest and cannot be adopted as a completed backup by its presence.
        backup.write_json(output / backup.MANIFEST, manifest)
        result = backup.verify_snapshot(output)
        backup.sync_directory(output)
        return result
