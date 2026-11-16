"""Creator-only CLI shared by the source workshop and standalone SDK."""
import argparse
import json
import sys

from mod_creator import create_project, import_audio, import_texture
from mod_package import ModError, build_package, inspect_package

COMMANDS = frozenset(('new', 'import-texture', 'import-audio', 'pack', 'validate'))


def add_commands(commands):
    new = commands.add_parser('new', help='Create a gameplay, texture, mixed or audio project')
    new.add_argument('destination')
    new.add_argument('--id', required=True)
    new.add_argument('--name', required=True)
    new.add_argument('--author', required=True, help='Your workshop username')
    new.add_argument('--template', choices=('gameplay', 'textures', 'mixed', 'audio'), default='gameplay')
    texture = commands.add_parser('import-texture', help='Import authored PNG/DDS without replacing source')
    texture.add_argument('project')
    texture.add_argument('source')
    texture.add_argument('--key', required=True)
    texture.add_argument('--format', choices=('DXT1', 'DXT5', 'RGBA8'))
    texture.add_argument('--width', type=int)
    texture.add_argument('--height', type=int)
    texture.add_argument('--channels', choices=('color', 'luminance', 'alpha'), default='color')
    audio = commands.add_parser('import-audio', help='Import exact PCM16 WAV into a keyed audio resource')
    audio.add_argument('project')
    audio.add_argument('source')
    audio.add_argument('--key', required=True)
    build = commands.add_parser('pack', help='Build a deterministic distributable ZIP')
    build.add_argument('source')
    build.add_argument('output')
    build.add_argument('--modules', action='store_true', help='Link project Lua modules into bounded main.lua')
    validate = commands.add_parser('validate', help='Check ZIP format, content and inventory without executing Lua')
    validate.add_argument('package')


def execute(args):
    if args.command == 'new':
        return create_project(args.destination, mod_id=args.id, name=args.name,
                              author=args.author, template=args.template)
    if args.command == 'import-texture':
        return import_texture(args.project, args.source, key=args.key, pixel_format=args.format,
                              width=args.width, height=args.height, channels=args.channels)
    if args.command == 'import-audio':
        return import_audio(args.project, args.source, key=args.key)
    if args.command == 'pack':
        return build_package(args.source, args.output, bundle_scripts=args.modules)
    if args.command == 'validate':
        return inspect_package(args.package)
    raise ModError('Unknown creator command')


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    add_commands(parser.add_subparsers(dest='command', required=True))
    args = parser.parse_args(argv)
    try:
        print(json.dumps(execute(args), indent=2))
        return 0
    except (ModError, OSError) as exc:
        print('Mod creator: '+str(exc), file=sys.stderr)
        return 2
    except KeyboardInterrupt:
        return 130
