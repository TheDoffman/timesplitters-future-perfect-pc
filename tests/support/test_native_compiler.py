"""Compiler discovery outside a Visual Studio developer shell."""


from types import SimpleNamespace
import pytest
import native_compiler as module


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
