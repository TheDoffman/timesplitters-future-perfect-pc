"""Link creator-owned Lua modules into the existing bounded main.lua handoff.

This is an authoring convenience, not an isolation boundary between modules.
All code belongs to one package/VM and retains the VM's existing permissions.
"""
import re

from mod_package import MAX_SCRIPT, require, validate_payload

MAX_SCRIPTS = 32
MAIN = 'scripts/main.lua'
MODULE = re.compile(r'[A-Za-z0-9_-]+(?:\.[A-Za-z0-9_-]+)*\Z')
PREFIX = b'''-- TSFP packaged Lua modules v1; no filesystem/package loader.
local __tsfp_type, __tsfp_error = type, error
local __tsfp_loaders, __tsfp_values, __tsfp_busy = {}, {}, {}
local function require(name)
  if __tsfp_type(name) ~= "string" then __tsfp_error("Module name must be a string", 2) end
  local loader = __tsfp_loaders[name]
  if loader == nil then __tsfp_error("Unknown packaged Lua module", 2) end
  if __tsfp_busy[name] then __tsfp_error("Cyclic packaged Lua module", 2) end
  local value = __tsfp_values[name]
  if value ~= nil then return value end
  __tsfp_busy[name] = true
  value = loader(require, name)
  __tsfp_busy[name] = nil
  if value == nil then value = true end
  __tsfp_values[name] = value
  return value
end
'''


def compose(scripts):
    """Return one UTF-8 source chunk; preserve a lone main.lua byte-for-byte."""
    require(isinstance(scripts, dict) and MAIN in scripts and 1 <= len(scripts) <= MAX_SCRIPTS,
            'Module projects require scripts/main.lua and at most32 Lua files')
    modules = {}
    seen = set()
    for path, source in scripts.items():
        require(isinstance(path, str) and isinstance(source, bytes), 'Invalid module source')
        require(path.startswith('scripts/') and path.lower().endswith('.lua'), 'Only packaged Lua sources can be linked')
        validate_payload(path, source)
        require(source, 'Empty module source')
        require(path.lower() not in seen, 'Case-colliding module paths')
        seen.add(path.lower())
        if path == MAIN:
            continue
        name = path[8:-4].replace('/', '.')
        require(MODULE.fullmatch(name) and name != 'main' and name not in modules,
                'Invalid or ambiguous packaged Lua module name')
        modules[name] = (path, source)
    for path in scripts:
        parents = path.lower().split('/')[:-1]
        require(all('/'.join(parents[:i]) not in seen for i in range(1, len(parents) + 1)),
                'Module file/directory collision')
    if not modules:
        return scripts[MAIN]
    parts, size = [PREFIX], len(PREFIX)
    for name, (path, source) in sorted(modules.items()):
        header = ('\n-- ' + path + '\n__tsfp_loaders["' + name + '"] = function(require, ...)\n').encode('ascii')
        part = header + source + b'\nend\n'
        size += len(part)
        require(size <= MAX_SCRIPT, 'Linked Lua source exceeds the1MiB runtime limit')
        parts.append(part)
    parts.append(b'\n-- scripts/main.lua\n' + scripts[MAIN])
    require(size + len(parts[-1]) <= MAX_SCRIPT, 'Linked Lua source exceeds the1MiB runtime limit')
    return b''.join(parts)
