"""Project JSON interchange and human-readable assembly listings."""
from contextlib import contextmanager
import json
from pathlib import Path
import re
import tempfile


@contextmanager
def atomic_text(path, root):
    path, root = Path(path), Path(root)
    if not path.resolve().is_relative_to(root.resolve()):
        raise ValueError('Analysis output escapes the selected directory')
    path.parent.mkdir(parents=True, exist_ok=True)
    pending = None
    try:
        with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', newline='\n',
                                         dir=path.parent, prefix='.analysis-', suffix='.tmp', delete=False) as stream:
            pending = Path(stream.name)
            yield stream
        pending.replace(path)
    finally:
        if pending is not None:
            pending.unlink(missing_ok=True)


class OutputWriter:
    def __init__(self, output_dir, engine, functions, xrefs, labels, image, strings):
        self.output_dir = Path(output_dir)
        self.engine, self.functions, self.xrefs = engine, functions, xrefs
        self.labels, self.image, self.strings = labels, image, strings

    def summary(self, sections=None):
        image, functions = self.image, self.functions.summary()
        selected = image.get_code_sections() if sections is None else sections
        return dict(binary=image.filepath, base_address=f'0x{image.base_address:08X}',
                    entry_point=f'0x{image.entry_point:08X}', total_instructions=self.engine.instruction_count(),
                    total_functions=functions['total_functions'], functions_with_prologue=functions['with_prologue'],
                    functions_by_method=functions['by_detection_method'], functions_by_section=functions['by_section'],
                    total_xrefs=self.xrefs.count(), xrefs_by_type=self.xrefs.count_by_type(),
                    total_labels=self.labels.count(), total_strings=len(self.strings), kernel_imports=len(image.kernel_imports),
                    sections_analyzed=[dict(name=section.name, va=f'0x{section.virtual_addr:08X}', size=section.virtual_size,
                                            functions=len(self.functions.get_functions_in_section(section.name)))
                                       for section in selected])

    def _strings(self):
        result = []
        for string in self.strings:
            record = dict(string, address=f"0x{string['address']:08X}")
            references = self.xrefs.get_refs_to(string['address'])
            if references:
                record['referenced_from'] = [f'0x{reference.from_addr:08X}' for reference in references]
            result.append(record)
        return result

    def _json(self, name, value):
        with atomic_text(self.output_dir / name, self.output_dir) as stream:
            json.dump(value, stream, indent=2, ensure_ascii=True)
            stream.write('\n')

    def write_all(self, sections_to_disasm=None, verbose=False):
        self._json('summary.json', self.summary(sections_to_disasm))
        self._json('functions.json', [function.to_dict() for _, function in sorted(self.functions.functions.items())])
        self._json('xrefs.json', self.xrefs.to_list())
        self._json('strings.json', self._strings())
        self._json('labels.json', self.labels.to_list())
        used = set()
        for section in sections_to_disasm or ():
            stem = re.sub('[^A-Za-z0-9_-]', '', section.name) or 'section'
            filename = stem + '.asm'
            if filename.casefold() in used:
                filename = f'{stem}_{section.virtual_addr:08X}.asm'
            serial = 1
            while filename.casefold() in used:
                filename = f'{stem}_{section.virtual_addr:08X}_{serial}.asm'
                serial += 1
            used.add(filename.casefold())
            self._listing(section, self.output_dir / 'asm' / filename)
        if verbose:
            print(f'Analysis written to {self.output_dir}')

    def _listing(self, section, path):
        functions = {function.start: function for function in self.functions.get_functions_in_section(section.name)}
        with atomic_text(path, self.output_dir) as stream:
            stream.write(f'; Section {json.dumps(section.name)} at 0x{section.virtual_addr:08X}, {section.virtual_size} bytes\n')
            for instruction in self.engine.get_instructions_in_range(section.virtual_addr, section.virtual_addr + section.virtual_size):
                address = instruction.address
                function = functions.get(address)
                if function is not None:
                    stream.write(f'\n; Function {function.name}: [0x{function.start:08X}, 0x{function.end:08X}) '
                                 f'evidence={function.detection_method}\n{function.name}:\n')
                elif self.labels.has(address):
                    stream.write(f'\n{self.labels.get_name(address)}:\n')
                destinations = []
                for target in (instruction.call_target, instruction.jump_target, instruction.memory_ref):
                    name = self.labels.get_name(target) if target is not None else None
                    if name and name not in destinations:
                        destinations.append(name)
                comment = ' ; ' + ', '.join(destinations) if destinations else ''
                stream.write(f'  0x{address:08X}  {instruction.bytes_hex:<30}  '
                             f'{instruction.mnemonic:<8} {instruction.op_str}{comment}\n')


def print_stats(engine, functions, xrefs, labels, strings, image):
    print(f'{Path(image.filepath).name}: {engine.instruction_count():,} instructions, '
          f'{len(functions.functions):,} functions, {xrefs.count():,} references, '
          f'{labels.count():,} labels, {len(strings):,} strings')
