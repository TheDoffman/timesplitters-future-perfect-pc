"""Compiler discovery outside a Visual Studio developer shell."""


from types import SimpleNamespace
import pytest
import native_compiler as module


@pytest.fixture(autouse=True)
def clear_discovery_cache():
    module._WINDOWS_ENVIRONMENTS.clear()
    yield
    module._WINDOWS_ENVIRONMENTS.clear()


def test_build_tools_and_mixed_case_path(tmp_path, monkeypatch):
    locator = tmp_path/'Microsoft Visual Studio/Installer/vswhere.exe'
    locator.parent.mkdir(parents=True)
    locator.write_bytes(b'fixture')
    install = tmp_path/'Standalone Build Tools'
    setup = install/'VC/Auxiliary/Build/vcvars64.bat'
    setup.parent.mkdir(parents=True)
    setup.write_text('fixture')
    monkeypatch.setattr(module, 'os', SimpleNamespace(name='nt', pathsep=';', environ={
        'ProgramFiles(x86)': str(tmp_path), 'Path': 'old path'}))
    monkeypatch.setattr(module.shutil, 'which', lambda name, path=None:
                        str(install/'cl.exe') if name == 'cl' and path == 'compiler path' else None)
    calls = []
    def output(command, **kwargs):
        calls.append(command)
        if isinstance(command, list):
            assert command[command.index('-products')+1] == '*'
            return str(install)+'\n'
        assert str(setup) in (tmp_path/'native-compiler-env.cmd').read_text()
        assert kwargs['env']['PATH'] == 'old path' and 'Path' not in kwargs['env']
        return 'Path=compiler path\nINCLUDE=include\nLIB=lib\nVSCMD_ARG_TGT_ARCH=x64\n=C:=ignored\n'
    monkeypatch.setattr(module.subprocess, 'check_output', output)
    compiler, environment = module.native_compiler(tmp_path)
    assert compiler == str(install/'cl.exe')
    assert environment['PATH'] == 'compiler path' and 'Path' not in environment
    assert len(calls) == 2


def test_ready_x64_shell_is_preserved(tmp_path, monkeypatch):
    environment = dict(Path='tools', INCLUDE='headers', LIB='libraries', VSCMD_ARG_TGT_ARCH='x64')
    monkeypatch.setattr(module, 'os', SimpleNamespace(name='nt', pathsep=';', environ=environment))
    monkeypatch.setattr(module.shutil, 'which', lambda name, path=None: 'cl.exe')
    compiler, inherited = module.native_compiler(tmp_path)
    assert compiler == 'cl.exe' and inherited['PATH'] == 'tools'
    assert environment['Path'] == 'tools'


def test_missing_toolchain_has_installation_guidance(tmp_path, monkeypatch):
    monkeypatch.setattr(module, 'os', SimpleNamespace(name='nt', pathsep=';', environ={'ProgramFiles(x86)': str(tmp_path)}))
    monkeypatch.setattr(module.shutil, 'which', lambda *args, **kwargs: None)
    with pytest.raises(AssertionError, match='Install Visual Studio or Build Tools'):
        module.native_compiler(tmp_path)


def test_posix_compiler_override_is_preserved(tmp_path, monkeypatch):
    monkeypatch.setattr(module, 'os', SimpleNamespace(name='posix', pathsep=':', environ={'CC': 'custom-cc', 'PATH': 'tools'}))
    monkeypatch.setattr(module.shutil, 'which', lambda name: '/tools/custom-cc' if name == 'custom-cc' else None)
    compiler, environment = module.native_compiler(tmp_path)
    assert compiler == '/tools/custom-cc' and environment['CC'] == 'custom-cc'


@pytest.fixture
def setup(tmp_path, monkeypatch):
    module._WINDOWS_ENVIRONMENTS.clear()
    locator = tmp_path/'Microsoft Visual Studio/Installer/vswhere.exe'
    locator.parent.mkdir(parents=True); locator.write_text('locator')
    install = tmp_path/'compiler'
    vcvars = install/'VC/Auxiliary/Build/vcvars64.bat'
    vcvars.parent.mkdir(parents=True); vcvars.write_text('setup')
    env = {'ProgramFiles(x86)': str(tmp_path), 'Path': 'original', 'PYTEST_CURRENT_TEST': 'first'}
    monkeypatch.setattr(module, 'os', SimpleNamespace(name='nt', pathsep=';', environ=env))
    monkeypatch.setattr(module.shutil, 'which', lambda name, path=None: 'cl.exe' if path == 'ready' else None)
    calls = []
    def output(command, **kwargs):
        calls.append(command)
        if isinstance(command, list): return str(install)
        return 'PATH=ready\nINCLUDE=headers\nLIB=libs\nVSCMD_ARG_TGT_ARCH=x64\n'
    monkeypatch.setattr(module.subprocess, 'check_output', output)
    return env, calls


def test_reuses_discovery_but_returns_fresh_environment(tmp_path, setup):
    env, calls = setup
    _, first = module.native_compiler(tmp_path)
    first['INCLUDE'] = 'mutated'; first['EXTRA'] = 'private'
    env['PYTEST_CURRENT_TEST'] = 'second'
    _, second = module.native_compiler(tmp_path/'does-not-need-a-script')
    assert len(calls) == 2
    assert second['INCLUDE'].startswith('headers;')
    assert 'EXTRA' not in second
    assert second['PYTEST_CURRENT_TEST'] == 'second'
    assert env['Path'] == 'original'
    del env['PYTEST_CURRENT_TEST']
    assert 'PYTEST_CURRENT_TEST' not in module.native_compiler(tmp_path)[1]


def test_changed_environment_reinitializes(tmp_path, setup):
    env, calls = setup
    module.native_compiler(tmp_path)
    env['Path'] = 'different'
    module.native_compiler(tmp_path)
    assert len(calls) == 4


def test_failed_setup_is_never_cached(tmp_path, setup, monkeypatch):
    _, calls = setup
    def broken(*args, **kwargs): raise RuntimeError('setup failed')
    with monkeypatch.context() as patch:
        patch.setattr(module.subprocess, 'check_output', broken)
        with pytest.raises(RuntimeError): module.native_compiler(tmp_path)
    module.native_compiler(tmp_path)
    assert len(calls) == 2


def test_discovery_cache_is_bounded(tmp_path, setup):
    env, calls = setup
    for index in range(10):
        env['Path'] = f'original-{index}'
        module.native_compiler(tmp_path)
    assert len(module._WINDOWS_ENVIRONMENTS) == 8
    assert len(calls) == 20
    env['Path'] = 'original-0'
    module.native_compiler(tmp_path)
    assert len(calls) == 22


def test_missing_cached_compiler_requires_successful_rediscovery(tmp_path, setup, monkeypatch):
    _, calls = setup
    module.native_compiler(tmp_path)
    monkeypatch.setattr(module.shutil, 'which', lambda *args, **kwargs: None)
    with pytest.raises(AssertionError, match='Could not initialize'):
        module.native_compiler(tmp_path)
    assert len(calls) == 4
    assert not module._WINDOWS_ENVIRONMENTS


def test_changed_working_directory_reinitializes(tmp_path, setup, monkeypatch):
    _, calls = setup
    module.native_compiler(tmp_path)
    monkeypatch.chdir(tmp_path)
    module.native_compiler(tmp_path)
    assert len(calls) == 4
