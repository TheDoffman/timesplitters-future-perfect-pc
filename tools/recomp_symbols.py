"""Project analysis symbols and references, independent of the toolkit.

The exported field names retain the analysis-file contract used by generation.
DATA_READ denotes a static memory operand; it does not assert an access mode.
"""
from collections import Counter, defaultdict
from dataclasses import dataclass
from enum import Enum
import re


class LabelType(Enum):
    KERNEL_IMPORT = 'kernel_import'
    FUNCTION = 'function'
    STRING_REF = 'string_ref'
    DATA = 'data'
    ENTRY_POINT = 'entry_point'
    THUNK = 'thunk'


@dataclass
class Label:
    address: int
    name: str
    label_type: LabelType
    section: str = ''
    confidence: float = 1.0

    def to_dict(self):
        return dict(address=f'0x{self.address:08X}', name=self.name,
                    type=self.label_type.value, section=self.section, confidence=self.confidence)


class LabelManager:
    def __init__(self):
        self._labels = {}
        self._names = defaultdict(dict)

    @staticmethod
    def _priority(label):
        return label.confidence, not label.name.startswith('sub_')

    def add(self, label):
        previous = self._labels.get(label.address)
        if previous is not None:
            if self._priority(previous) >= self._priority(label):
                return
            self._names[previous.name].pop(previous.address, None)
        self._labels[label.address] = label
        self._names[label.name][label.address] = label

    def get(self, address):
        return self._labels.get(address)

    def get_by_name(self, name):
        entries = self._names.get(name)
        return entries[next(reversed(entries))] if entries else None

    def has(self, address):
        return address in self._labels

    def get_name(self, address):
        label = self.get(address)
        return label.name if label is not None else None

    def get_display_name(self, address):
        return self.get_name(address) or f'0x{address:08X}'

    def auto_name_function(self, address, section='', confidence=0.5):
        label = self.get(address)
        if label is not None and label.label_type == LabelType.FUNCTION:
            return label
        proposed = Label(address, f'sub_{address:08X}', LabelType.FUNCTION, section, confidence)
        self.add(proposed)
        return proposed

    def all_labels(self):
        return [self._labels[address] for address in sorted(self._labels)]

    def labels_in_range(self, start, end):
        return [label for address, label in self._labels.items() if start <= address < end]

    def count(self):
        return len(self._labels)

    def count_by_type(self, label_type):
        return sum(label.label_type == label_type for label in self._labels.values())

    def to_list(self):
        return [label.to_dict() for label in self.all_labels()]


def populate_kernel_labels(labels, image):
    for entry in image.kernel_imports:
        labels.add(Label(entry.thunk_addr, 'xbox_' + entry.name, LabelType.KERNEL_IMPORT, '.rdata'))
    return len(image.kernel_imports)


def populate_entry_point(labels, image):
    labels.add(Label(image.entry_point, 'xbe_entry_point', LabelType.ENTRY_POINT, '.text'))


def extract_strings(image, min_length=4, max_length=256):
    if min_length < 1 or max_length < min_length:
        raise ValueError('String bounds require 1 <= minimum <= maximum')
    section = image.get_section('.rdata')
    if section is None:
        return []
    result = []
    # Consume each complete printable run; an overlong run must not produce a
    # spurious suffix string merely because a scanning window ended inside it.
    for match in re.finditer(rb'[\x09\x0a\x0d\x20-\x7e]+\x00', image.get_section_data(section)):
        content = match.group()[:-1]
        if min_length <= len(content) <= max_length:
            result.append(dict(address=section.virtual_addr + match.start(),
                               string=content.decode('ascii'), length=len(content)))
    return result


def populate_string_labels(labels, string_refs):
    for entry in string_refs:
        content = entry['string'][:32].replace(' ', '_').replace('/', '_')
        name = ''.join(character for character in content if character.isalnum() or character == '_')
        labels.add(Label(entry['address'], 'str_' + (name or f"{entry['address']:08X}"),
                         LabelType.STRING_REF, '.rdata', 0.8))
    return len(string_refs)


class XRefType(Enum):
    CALL = 'call'
    JUMP = 'jump'
    COND_JUMP = 'cond_jump'
    DATA_READ = 'data_read'
    DATA_IMM = 'data_imm'
    KERNEL_CALL = 'kernel_call'


@dataclass
class XRef:
    from_addr: int
    to_addr: int
    xref_type: XRefType
    kernel_name: str | None = None

    def to_dict(self):
        result = dict(type=self.xref_type.value, to=f'0x{self.to_addr:08X}')
        result['from'] = f'0x{self.from_addr:08X}'
        if self.kernel_name:
            result['kernel_name'] = self.kernel_name
        return result


class XRefTracker:
    def __init__(self):
        self._outgoing = defaultdict(list)
        self._incoming = defaultdict(list)
        self._kernel = defaultdict(list)
        self._counts = Counter()

    def add(self, xref):
        self._outgoing[xref.from_addr].append(xref)
        self._incoming[xref.to_addr].append(xref)
        self._counts[xref.xref_type.value] += 1
        if xref.xref_type == XRefType.KERNEL_CALL:
            self._kernel[xref.to_addr].append(xref.from_addr)

    def get_refs_from(self, address):
        return list(self._outgoing.get(address, ()))

    def get_refs_to(self, address):
        return list(self._incoming.get(address, ()))

    def get_callers(self, address):
        return [ref.from_addr for ref in self.get_refs_to(address) if ref.xref_type == XRefType.CALL]

    def get_callees(self, address):
        return [ref.to_addr for ref in self.get_refs_from(address) if ref.xref_type == XRefType.CALL]

    def get_kernel_call_sites(self, address):
        return list(self._kernel.get(address, ()))

    def all_kernel_calls(self):
        return {address: list(sites) for address, sites in self._kernel.items()}

    def count(self):
        return sum(self._counts.values())

    def count_by_type(self):
        return dict(self._counts)

    def to_list(self):
        return [ref.to_dict() for address in sorted(self._outgoing) for ref in self._outgoing[address]]


def build_xrefs(engine, image):
    result = XRefTracker()
    for instruction in engine.instructions.values():
        address = instruction.address
        if instruction.is_call and instruction.call_target is not None:
            result.add(XRef(address, instruction.call_target, XRefType.CALL))
        if instruction.is_call or instruction.is_branch:
            if instruction.memory_ref is not None:
                entry = image.get_kernel_import_at_thunk(instruction.memory_ref)
                if entry is not None:
                    result.add(XRef(address, instruction.memory_ref, XRefType.KERNEL_CALL, entry.name))
                elif instruction.is_call:
                    result.add(XRef(address, instruction.memory_ref, XRefType.CALL))
            if instruction.jump_target is not None:
                kind = XRefType.COND_JUMP if instruction.is_cond_jump else XRefType.JUMP
                result.add(XRef(address, instruction.jump_target, kind))
        else:
            for target, kind in ((instruction.memory_ref, XRefType.DATA_READ),
                                 (instruction.imm_ref, XRefType.DATA_IMM)):
                if target is not None:
                    result.add(XRef(address, target, kind))
    return result
