"""Immutable encrypted snapshot exports and receipt-last private reception.

Receipts must arrive over an independently authenticated transport. They contain
no plaintext inventory/hash, secrets or paths. These operations do not schedule
copies, delete older backups or make a restored repository publishable.
"""
import hashlib
from itertools import islice
import os
from pathlib import Path
import re
import shutil
import struct
import time
import uuid

import mod_backup_archive as archive
import mod_backup_cipher as cipher
import mod_backup_recipient as recipients
from mod_package import read_json, require
from mod_trust import encode

TYPE = 'tsfp-encrypted-backup-export-v1'
RECEIPT = 'receipt.json'
PAYLOAD = 'payload.bin'
MAX_RECEIPT = 4096
MAGIC = b'TSFPBT1\n'


def new_directory(path):
    path = cipher.private_path(path)
    if os.name == 'nt':
        from mod_windows_recovery import private
        return private(path, create=True)
    require(not path.exists() and not path.is_symlink(), 'Preserving existing backup export destination')
    path.mkdir(mode=0o700)
    return path


def sync(path):
    if os.name == 'posix':
        from mod_backup import sync_directory
        sync_directory(path)


def checked_receipt(wire, recipient, service_origin, pin):
    recipients.checked_public(recipient, service_origin, pin)
    require(isinstance(wire, bytes) and 0 < len(wire) <= MAX_RECEIPT, 'Backup receipt exceeds budget')
    value = read_json(wire)
    require(isinstance(value, dict) and set(value) ==
            {'schema', 'type', 'export_id', 'created', 'origin', 'trusted_root_sha256',
             'key_id', 'cipher_bytes', 'cipher_sha256'} and encode(value) == wire,
            'Unknown or noncanonical backup receipt')
    require(type(value['schema']) is int and value['schema'] == 1 and value['type'] == TYPE
            and isinstance(value['export_id'], str) and re.fullmatch('[0-9a-f]{32}', value['export_id'])
            and type(value['created']) is int and 0 <= value['created'] < 10**12,
            'Invalid backup export identity')
    require(value['origin'] == service_origin and value['trusted_root_sha256'] == pin
            and value['key_id'] == recipient['key_id'], 'Backup receipt authority differs')
    require(type(value['cipher_bytes']) is int and 0 < value['cipher_bytes'] <= cipher.MAX_WIRE,
            'Backup ciphertext exceeds budget')
    cipher.checked_sha(value['cipher_sha256'])
    return value


def bounded_read(stream, length):
    require(type(length) is int and 0 <= length <= cipher.CHUNK, 'Unbounded backup transfer read')
    value = bytearray()
    while len(value) < length:
        data = stream.read(length - len(value))
        require(isinstance(data, bytes) and 0 < len(data) <= length - len(value), 'Truncated backup transfer')
        value.extend(data)
    return bytes(value)


def verify_payload(path, receipt, recipient, service_origin, pin):
    with cipher.incoming(path, cipher.MAX_WIRE) as (stream, size, stable):
        require(size == receipt['cipher_bytes'], 'Backup ciphertext size differs')
        checksum = hashlib.sha256()
        remaining = size
        while remaining:
            data = bounded_read(stream, min(cipher.CHUNK, remaining))
            checksum.update(data)
            remaining -= len(data)
        require(stream.read(1) == b'' and checksum.hexdigest() == receipt['cipher_sha256'],
                'Backup ciphertext differs from authenticated receipt')
        stable()
        stream.seek(0)
        require(bounded_read(stream, len(cipher.MAGIC)) == cipher.MAGIC, 'Unknown backup ciphertext magic')
        length = struct.unpack('>I', bounded_read(stream, 4))[0]
        require(0 < length <= cipher.MAX_HEADER, 'Backup ciphertext header exceeds budget')
        header = cipher.checked_header(bounded_read(stream, length), service_origin, pin)
        require(header['recipient'] == recipient and cipher.wire_size(header, length) == size,
                'Backup ciphertext recipient or framing differs')
        stable()


def write_receipt(folder, wire):
    # Payload bytes/hash/profile and transport status must already be verified.
    sync(folder)
    with cipher.outgoing(folder / RECEIPT, len(wire)) as output:
        output.write(wire)
    sync(folder)
    sync(folder.parent)


def export_snapshot(source, destination, recipient, service_origin, pin, *, clock=time.time):
    recipients.checked_public(recipient, service_origin, pin)
    created = int(clock())
    require(0 <= created < 10**12, 'Invalid backup export clock')
    destination = cipher.private_path(destination)
    source = Path(os.path.abspath(source))
    require(not destination.is_relative_to(source) and not source.is_relative_to(destination),
            'Snapshot and encrypted export must be separate')
    folder = new_directory(destination)
    report = archive.encrypt_snapshot(source, folder / PAYLOAD, recipient, service_origin, pin)
    value = dict(schema=1, type=TYPE, export_id=uuid.uuid4().hex, created=created,
                 origin=service_origin, trusted_root_sha256=pin, key_id=recipient['key_id'],
                 cipher_bytes=report['bytes'], cipher_sha256=report['cipher_sha256'])
    wire = encode(value)
    checked_receipt(wire, recipient, service_origin, pin)
    verify_payload(folder / PAYLOAD, value, recipient, service_origin, pin)
    write_receipt(folder, wire)
    return value


def inspect_export(folder, recipient, service_origin, pin):
    folder = cipher.private_path(Path(folder) / PAYLOAD).parent
    with os.scandir(folder) as entries:
        names = [entry.name for entry in islice(entries, 3)]
    require(len(names) == 2 and set(names) == {PAYLOAD, RECEIPT},
            'Preserving incomplete or foreign backup export')
    with cipher.incoming(folder / RECEIPT, MAX_RECEIPT) as (stream, size, stable):
        wire = bounded_read(stream, size)
        require(stream.read(1) == b'', 'Backup receipt grew')
        stable()
    value = checked_receipt(wire, recipient, service_origin, pin)
    verify_payload(folder / PAYLOAD, value, recipient, service_origin, pin)
    return folder, wire, value


def send_stream(folder, output, recipient, service_origin, pin):
    folder, wire, receipt = inspect_export(folder, recipient, service_origin, pin)
    def write(data):
        require(output.write(data) == len(data), 'Short backup transfer write')
    write(MAGIC + struct.pack('>I', len(wire)) + wire)
    checksum = hashlib.sha256()
    with cipher.incoming(folder / PAYLOAD, cipher.MAX_WIRE) as (stream, size, stable):
        require(size == receipt['cipher_bytes'], 'Backup changed before transfer')
        remaining = size
        while remaining:
            data = bounded_read(stream, min(cipher.CHUNK, remaining))
            write(data)
            checksum.update(data)
            remaining -= len(data)
        require(stream.read(1) == b'' and checksum.hexdigest() == receipt['cipher_sha256'],
                'Backup changed during transfer')
        stable()
    output.flush()
    return receipt


def receive_stream(stream, destination, recipient, service_origin, pin, *, verify_transport):
    require(callable(verify_transport), 'Authenticated transport completion check is required')
    require(bounded_read(stream, len(MAGIC)) == MAGIC, 'Unknown backup transfer magic')
    length = struct.unpack('>I', bounded_read(stream, 4))[0]
    require(0 < length <= MAX_RECEIPT, 'Backup transfer receipt exceeds budget')
    wire = bounded_read(stream, length)
    value = checked_receipt(wire, recipient, service_origin, pin)
    destination = cipher.private_path(destination)
    require(shutil.disk_usage(destination.parent).free >= value['cipher_bytes'] + len(wire) + cipher.MIN_FREE,
            'Insufficient backup reception headroom')
    folder = new_directory(destination)
    checksum = hashlib.sha256()
    with cipher.outgoing(folder / PAYLOAD, value['cipher_bytes']) as output:
        remaining = value['cipher_bytes']
        while remaining:
            data = bounded_read(stream, min(cipher.CHUNK, remaining))
            output.write(data)
            checksum.update(data)
            remaining -= len(data)
        require(stream.read(1) == b'' and checksum.hexdigest() == value['cipher_sha256'],
                'Backup transfer content or boundary differs')
    # Sender may fail after producing valid bytes. A zero exit/deadline/diagnostic
    # check on the authenticated transport is mandatory before committing receipt.
    verify_transport()
    verify_payload(folder / PAYLOAD, value, recipient, service_origin, pin)
    write_receipt(folder, wire)
    return value
