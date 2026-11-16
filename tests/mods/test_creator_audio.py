"""Creator WAV preflight parity with the independently compiled native boundary."""
from pathlib import Path
import random
import struct

import pytest

from mod_creator import create_project, example_wave, import_audio
from mod_package import ModError
from mod_wave_input import inspect_wave
from test_wave_input import native,parse,container,chunk,fmt


def test_creator_and_real_native_parser_agree_on_metadata_and_mutated_containers(native):
    wave=example_wave(50);rng=random.Random(54173)
    corpus=[wave,container(chunk(b'JUNK',b'odd'),fmt(channels=2),chunk(b'data',b'\0'*8)),
        container(fmt(extra=b'\0\0'),chunk(b'data',b'\0\0'))]
    for at in range(44):corpus.append(wave[:at])
    for _ in range(500):
        value=bytearray(wave);at=rng.randrange(min(len(value),128));value[at]^=1<<rng.randrange(8);corpus.append(bytes(value))
    corpus += [container(fmt(**fields),chunk(b'data',b'\0\0')) for fields in
        [dict(code=3),dict(code=0xfffe),dict(channels=0),dict(channels=3),dict(rate=7999),dict(rate=48001),dict(bits=8),dict(align=4),dict(byte_rate=1)]]
    corpus += [container(fmt(),chunk(b'data',b'\0\0'),fmt()),container(chunk(b'data',b'\0\0'),fmt()),
        container(fmt(),chunk(b'data',b'\0')),container(fmt(),chunk(b'data',b'')),
        container(*[chunk(b'JUNK',b'')]*63,fmt(),chunk(b'data',b'\0\0'))]
    for wire in corpus:
        error,view,pcm=parse(native,wire)
        try:metadata=inspect_wave(wire)
        except ModError:
            assert error!=0
        else:
            assert error==0
            assert (metadata['rate'],metadata['channels'],metadata['frames'],metadata['pcm_bytes'])==(view.rate,view.channels,view.frames,view.bytes)
            assert wire[metadata['pcm_offset']:metadata['pcm_offset']+metadata['pcm_bytes']]==pcm


@pytest.mark.parametrize('key',['../escape','0000000A','ffffffff','0123456789abcdef','x',''])
def test_bad_audio_keys_preserve_inputs_and_project(tmp_path,key):
    project=tmp_path/'project';create_project(project,mod_id='own-mode',name='Own mode',author='creator')
    source=tmp_path/'own.wav';source.write_bytes(example_wave(25))
    with pytest.raises(ModError):import_audio(project,source,key=key)
    assert not (project/'audio').exists() and source.read_bytes()==example_wave(25)


@pytest.mark.parametrize('wire',[b'MZexecutable',b'OggScompressed',b'RIFF0000WAVE',
    container(fmt(code=3),chunk(b'data',b'\0\0')),container(fmt(),chunk(b'data',b'\0'))])
def test_unsupported_audio_does_not_leave_partial_payload(tmp_path,wire):
    project=tmp_path/'project';create_project(project,mod_id='own-mode',name='Own mode',author='creator')
    source=tmp_path/'own.wav';source.write_bytes(wire)
    with pytest.raises(ModError):import_audio(project,source,key='00000003')
    assert not (project/'audio').exists() and source.read_bytes()==wire


def test_audio_import_preserves_exact_metadata_and_pcm_and_refuses_collisions(tmp_path):
    project=tmp_path/'project';create_project(project,mod_id='own-mode',name='Own mode',author='creator')
    source=tmp_path/'own.wav';wire=container(chunk(b'LIST',b'original metadata'),fmt(),chunk(b'data',struct.pack('<3h',-32768,0,32767)))
    source.write_bytes(wire)
    info=import_audio(project,source,key='00000003')
    assert info['frames']==3 and info['channels']==1 and info['rate']==22050
    assert Path(info['path']).read_bytes()==source.read_bytes()==wire
    with pytest.raises(ModError,match='already exists'):import_audio(project,source,key='00000003')
