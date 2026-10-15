"""Exercise real runtime patches on small source-only checkout fixtures."""
from pathlib import Path
import importlib.util
import subprocess
import sys

import pytest

ROOT = Path(__file__).resolve().parents[1]
PATCHES = ROOT / 'patches'
RUNTIME = ROOT / 'external/ts3FPrecomp'
spec = importlib.util.spec_from_file_location('apply_patches', ROOT / 'tools/apply-patches.py')
installer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(installer)


@pytest.fixture
def checkout(tmp_path):
    subprocess.run(['git', 'init', '-q', str(tmp_path)], check=True)
    names = {line[6:] for patch in PATCHES.glob('*.patch')
             for line in patch.read_text(encoding='utf-8').splitlines()
             if line.startswith('+++ b/')}
    for name in names:
        destination = tmp_path / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(subprocess.check_output(
            ['git', '-C', str(RUNTIME), 'show', 'HEAD:' + name]))
    return tmp_path


def apply(tree, patch):
    result = installer.git(tree, 'apply', PATCHES / patch)
    assert result.returncode == 0, result.stderr


def snapshot(tree):
    return {p.relative_to(tree): p.read_bytes() for folder in ('src', 'titles', 'tools')
            for p in (tree / folder).rglob('*') if p.is_file()}


def test_fresh_checkout_and_repeated_build(checkout):
    assert installer.apply_patches(checkout, PATCHES)
    complete = snapshot(checkout)
    assert installer.apply_patches(checkout, PATCHES) == []
    assert snapshot(checkout) == complete


def test_upgrade_previous_controls_with_newer_input_features(checkout):
    # The user's state: controls/debug/prompts applied, display/textures missing.
    for patch in sorted(PATCHES.glob('*.patch')):
        if patch.name not in ('pc-display.patch', 'texture-pack.patch', 'texture-toggle.patch'):
            apply(checkout, patch.name)
    manual = checkout / 'titles/futureperfect/src/recomp_manual.c'
    manual.write_bytes(manual.read_bytes() + b'\r\n/* unrelated local edit */\r\n')
    assert installer.apply_patches(checkout, PATCHES)
    assert b'pc_display_camera' in manual.read_bytes()
    assert b'pc_prompt_guest' in manual.read_bytes()
    assert manual.read_bytes().endswith(b'/* unrelated local edit */\r\n')
    assert installer.apply_patches(checkout, PATCHES) == []


def test_already_integrated_controls_and_display(checkout):
    # Same result as the previously combined pc-controls.patch from main.
    apply(checkout, 'pc-controls.patch')
    apply(checkout, 'pc-display.patch')
    assert installer.apply_patches(checkout, PATCHES)
    assert installer.apply_patches(checkout, PATCHES) == []


def test_conflict_preserves_every_runtime_file(checkout):
    apply(checkout, 'pc-controls.patch')
    target = checkout / 'src/input/input_bindings.c'
    target.write_bytes(target.read_bytes().replace(
        b'pc_input_normalize_movement', b'custom_normalize_movement'))
    before = snapshot(checkout)
    with pytest.raises(RuntimeError, match='pc-controls.patch conflicts'):
        installer.apply_patches(checkout, PATCHES)
    assert snapshot(checkout) == before


def test_windows_line_endings(checkout):
    apply(checkout, 'pc-controls.patch')
    for name, content in snapshot(checkout).items():
        (checkout / name).write_bytes(content.replace(b'\r\n', b'\n').replace(b'\n', b'\r\n'))
    assert installer.apply_patches(checkout, PATCHES)
    assert installer.apply_patches(checkout, PATCHES) == []


@pytest.mark.skipif(sys.platform != 'win32', reason='Windows PowerShell stderr behavior')
@pytest.mark.parametrize('exit_code', [0, 7])
def test_checked_command_uses_exit_status_with_redirected_stderr(exit_code, tmp_path):
    def quote(value):
        return "'" + str(value).replace("'", "''") + "'"

    native = tmp_path / 'native warning.py'
    native.write_text('import sys\nsys.stderr.write("warning\\n")\nsys.exit(int(sys.argv[1]))\n',
                      encoding='utf-8')
    script = f"""
$ErrorActionPreference = 'Stop'
$ast = [System.Management.Automation.Language.Parser]::ParseFile(
    {quote(ROOT / 'tsfp.ps1')}, [ref]$null, [ref]$null)
$function = $ast.Find({{ param($node)
    $node -is [System.Management.Automation.Language.FunctionDefinitionAst] -and
    $node.Name -eq 'Invoke-Checked'
}}, $true)
. ([scriptblock]::Create($function.Extent.Text))
try {{
    $output = Invoke-Checked {quote(sys.executable)} @({quote(native)}, '{exit_code}') *>&1
    if ({exit_code} -ne 0) {{ throw 'Nonzero exit was accepted' }}
    if ($ErrorActionPreference -ne 'Stop') {{ throw 'Preference was not restored' }}
    if (($output | Out-String) -notmatch 'warning') {{ throw 'Warning was lost' }}
}} catch {{
    if ({exit_code} -eq 0 -or $_.Exception.Message -notmatch 'failed \\(exit {exit_code}\\)') {{
        throw
    }}
    if ($ErrorActionPreference -ne 'Stop') {{ throw 'Preference was not restored' }}
}}
exit 0
"""
    result = subprocess.run(['powershell.exe', '-NoProfile', '-Command', script],
                            capture_output=True, text=True)
    assert result.returncode == 0, result.stdout + result.stderr
