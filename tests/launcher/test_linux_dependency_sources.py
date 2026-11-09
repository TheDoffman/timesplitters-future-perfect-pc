"""Corresponding source indexes must match exact versions and reject unsafe names."""
import pytest
from linux_dependency_sources import source_files

def text(name='source.tar.xz',version='1.2-3',sha='a'*64):
    return f'Package: example\nVersion: {version}\nChecksums-Sha256:\n {sha} 12 {name}\n'

def test_exact_source_version_and_hashes():
    index=text(version='1.2-2')+'\n'+text()
    assert source_files(index,'1.2-3')=={'source.tar.xz':('a'*64,12)}
    with pytest.raises(ValueError,match='unavailable'):source_files(index,'9.0')

@pytest.mark.parametrize('name,sha',[('../escape','a'*64),('/absolute','a'*64),('good','invalid')])
def test_source_index_rejects_traversal_or_invalid_digest(name,sha):
    with pytest.raises(ValueError,match='Unsafe'):source_files(text(name=name,sha=sha),'1.2-3')
