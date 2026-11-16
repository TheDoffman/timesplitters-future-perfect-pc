"""The production public pin can stage launcher settings but cannot imply release."""
import importlib.util
import base64
import json
from pathlib import Path
import re

import pytest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('public_host_profile',ROOT/'tools/mods/hosting/prepare-launcher-workshop-profile.py')
profile=importlib.util.module_from_spec(spec);spec.loader.exec_module(profile)
from mod_package import ModError
from mod_trust import root_document


@pytest.fixture
def files(tmp_path,monkeypatch):
    public=tmp_path/'root.json';settings=tmp_path/'profile.json'
    public.write_bytes(profile.PUBLIC_ROOT.read_bytes());settings.write_bytes(profile.PROFILE.read_bytes())
    monkeypatch.setattr(profile,'PUBLIC_ROOT',public);monkeypatch.setattr(profile,'PROFILE',settings)
    root=root_document(public.read_bytes())
    return public,settings,root['issued']+1


def test_source_pinned_production_root_has_expected_live_authority(files):
    _,_,now=files
    settings,wire=profile.validate(now)
    assert settings['origin']=='https://dans-mac-mini.tail7e8ab8.ts.net:8443'
    assert settings['trusted_root_sha256']=='b52e5d499648b84e8f5e84bb7b0a539426e6ff706f396d94baf7dfee19f73d27'
    root=root_document(wire)
    assert root['roles']['root']['threshold']==2 and root['roles']['catalog']['threshold']==1
    assert settings['status']=='active'


def test_compiled_production_defaults_match_the_accepted_profile(files):
    public,_,now=files
    settings,_=profile.validate(now)
    source=(ROOT/'tools/launcher/ModTrustPage.cs').read_text(encoding='utf-8')
    origin=re.search(r'internal const string DefaultOrigin="([^"]+)";',source).group(1)
    wire=base64.b64decode(re.search(r'internal const string DefaultRootBase64="([^"]+)";',source).group(1),validate=True)
    assert origin==settings['origin'] and wire==public.read_bytes()


def test_active_profile_stages_a_release_profile_without_acceptance_override(files,tmp_path):
    _,_,now=files
    result=profile.prepare(tmp_path/'release-profile',now=now)
    assert result['phase']=='release-profile' and result['public_acceptance_complete']


def test_pending_profile_only_stages_for_private_acceptance_and_never_overwrites(files,tmp_path):
    _,settings,now=files
    value=json.loads(settings.read_bytes());value['status']='pending-public-acceptance'
    settings.write_text(json.dumps(value))
    output=tmp_path/'profile'
    with pytest.raises(ModError,match='incomplete'):profile.prepare(output,now=now)
    assert not output.exists()
    receipt=profile.prepare(output,acceptance_only=True,now=now)
    assert receipt['phase']=='acceptance-only' and receipt['public_acceptance_complete'] is False
    assert receipt['services_started'] is receipt['network_started'] is False
    assert set(p.name for p in output.iterdir())=={'service.json','trusted-root.json','profile-receipt.json'}
    assert json.loads((output/'service.json').read_bytes())==dict(service=receipt['origin'])
    assert (output/'trusted-root.json').read_bytes()==profile.PUBLIC_ROOT.read_bytes()
    before={p.name:p.read_bytes() for p in output.iterdir()}
    with pytest.raises(ModError,match='Preserving'):profile.prepare(output,acceptance_only=True,now=now)
    assert before=={p.name:p.read_bytes() for p in output.iterdir()}


@pytest.mark.parametrize('damage',['origin','pin','schema','status','extra','root-bytes','expired'])
def test_changed_invalid_or_expired_profile_is_refused_before_output(files,tmp_path,damage):
    public,settings,now=files
    value=json.loads(settings.read_bytes())
    if damage=='origin':value['origin']='https://other.example.test'
    if damage=='pin':value['trusted_root_sha256']='0'*64
    if damage=='schema':value['schema']=True
    if damage=='status':value['status']='released-without-acceptance'
    if damage=='extra':value['root_key']='forbidden'
    if damage=='root-bytes':public.write_bytes(public.read_bytes()+b' ')
    if damage=='expired':now=root_document(public.read_bytes())['expires']
    settings.write_text(json.dumps(value))
    with pytest.raises(ModError):profile.prepare(tmp_path/'output',acceptance_only=True,now=now)
    assert not (tmp_path/'output').exists()
