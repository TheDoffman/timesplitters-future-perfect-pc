"""Exercise actual Linux memory adapter branches with controlled host responses."""
from pathlib import Path
import os
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tests/compiler'))
from test_translation_product import _compile


def execute(tmp_path, harness, headers, sources):
    for name, text in headers.items():
        path = tmp_path / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text, encoding='utf-8')
    for name, source in sources.items():
        (tmp_path / name).write_bytes(source.read_bytes())
    (tmp_path / harness).write_bytes((ROOT / 'tests/compiler' / harness).read_bytes())
    flags = ['/W4', '/D_CRT_SECURE_NO_WARNINGS', '/I' + str(tmp_path)] if os.name == 'nt' else ['-Wall', '-Wextra', '-I' + str(tmp_path)]
    exe = _compile(tmp_path, tmp_path / harness, extra=flags)
    result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=30)
    assert result.returncode == 0, result.stdout + result.stderr
    assert 'passed' in result.stdout


def test_linux_containing_vma_and_holes(tmp_path):
    execute(tmp_path, 'linux_host_pages_model.c', {
        'unistd.h': 'long sysconf(int);\n#define _SC_PAGESIZE 1\n',
        'sys/mman.h': '''#include <stdint.h>
#include <stddef.h>
#define PROT_NONE 0
#define PROT_READ 1
#define PROT_WRITE 2
#define PROT_EXEC 4
#define MAP_PRIVATE 2
#define MAP_ANON 4096
#define MAP_FAILED ((void *)(intptr_t)-1)
void *mmap(void *,size_t,int,int,int,int64_t);
int munmap(void *,size_t);
int mprotect(void *,size_t,int);
''',
    }, {'host_pages.c': ROOT / 'src/native/host_pages.c',
        'host_pages.h': ROOT / 'src/runtime/host_pages.h'})


def test_linux_free_memory_units_overflow_and_failures(tmp_path):
    execute(tmp_path, 'linux_host_stats_model.c', {
        'sys/sysinfo.h': '''#include <stdint.h>
struct sysinfo { uint64_t freeram; unsigned mem_unit; };
int sysinfo(struct sysinfo *);
''',
    }, {'host_memory_stats.c': ROOT / 'src/native/host_memory_stats.c',
        'host_memory_stats.h': ROOT / 'src/runtime/host_memory_stats.h',
        'host_pages.h': ROOT / 'src/runtime/host_pages.h'})
