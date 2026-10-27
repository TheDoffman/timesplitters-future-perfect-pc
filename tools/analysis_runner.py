"""Standalone project XBE analysis pipeline and command-line interface."""
import argparse
from dataclasses import replace
import json
from pathlib import Path
import sys
import time

from analysis_cache import AnalysisCache
from analysis_config import DEFAULT_OUTPUT_DIR
from analysis_output import OutputWriter, print_stats
from recomp_image import load_image, _find_analysis_json
from recomp_symbols import (LabelManager, build_xrefs, extract_strings, populate_entry_point,
                            populate_kernel_labels, populate_string_labels)
from x86_functions import FunctionDetector
from x86_recovery import CodeRecovery


def _address(value):
    if isinstance(value, bool) or not isinstance(value, (int, str)):
        raise ValueError('A seed address must be an integer or hexadecimal string')
    address = int(value, 16) if isinstance(value, str) else value
    if not 0 <= address < 2**32:
        raise ValueError('A seed address must fit the 32-bit guest address space')
    return address


def _load_seed_functions(path, observed=None):
    path = Path(path)
    if path.stat().st_size > 64 * 1024 * 1024:
        raise ValueError('Seed file exceeds 64 MiB')
    entries = json.loads(path.read_text(encoding='utf-8'))
    if not isinstance(entries, list) or len(entries) > 1_000_000:
        raise ValueError('Seed file must contain a bounded array')
    addresses = []
    for entry in entries:
        if isinstance(entry, dict):
            if 'start' not in entry or not isinstance(entry.get('observed', False), bool):
                raise ValueError('Invalid seed entry')
            address = _address(entry['start'])
            note = entry.get('note', '')
            if not isinstance(note, str):
                raise ValueError('Seed note must be text')
            if observed is not None and (entry.get('observed') or 'observed at runtime' in note):
                observed.add(address)
        else:
            address = _address(entry)
        addresses.append(address)
    return addresses


class Disassembler:
    def __init__(self, xbe_path, analysis_json=None, output_dir=None, text_only=False,
                 stats_only=False, verbose=False, force=False, extra_sections=None,
                 seed_functions=None, observed_seeds=None):
        self.xbe_path, self.analysis_json = xbe_path, analysis_json
        self.output_dir = output_dir or DEFAULT_OUTPUT_DIR
        self.text_only, self.stats_only, self.verbose, self.force = text_only, stats_only, verbose, force
        self.extra_sections = sorted(set(extra_sections or ()))
        self.seed_functions = sorted({_address(address) for address in seed_functions or ()})
        self.observed_seeds = {_address(address) for address in observed_seeds or ()}
        self.image = self.engine = self.labels = self.xrefs = self.func_detector = None
        self.strings = []
        self.cache_hit = False

    def _find_analysis_json(self):
        return self.analysis_json if self.analysis_json is not None else _find_analysis_json(self.xbe_path)

    def _get_target_sections(self):
        # Explicit selection is an analysis overlay, not a mutation of file
        # permissions or of the validated immutable section records.
        overrides = set(self.extra_sections)
        for name in overrides:
            section = self.image.get_section(name)
            if section is None or not section.raw_size:
                raise ValueError(f'Extra code section is missing or empty: {name}')
        if overrides:
            self.image = replace(self.image, sections=[replace(section, executable=True)
                                  if section.name in overrides else section for section in self.image.sections])
        if self.text_only:
            section = self.image.get_section('.text')
            if section is None:
                raise ValueError('No .text section found')
            selected = [section]
        else:
            selected = self.image.get_code_sections()
        present = {section.name for section in selected}
        selected.extend(section for section in self.image.sections
                        if section.name in overrides and section.name not in present)
        return selected

    def _seed(self):
        accepted = rejected = 0
        for address in self.seed_functions:
            section = self.image.get_section_at_va(address)
            if section is None or not section.executable:
                rejected += 1
                continue
            overlapping = self.engine.instruction_covering(address) is not None
            if overlapping:
                evidence = (address in self.observed_seeds or self.engine.probes_as_prologue(address)
                            or self.engine.follows_padding(address))
                if not evidence or not self.engine.decode_at(address):
                    rejected += 1
                    continue
            elif address not in self.engine.instructions:
                self.engine.decode_at(address)
            self.func_detector._add_candidate(address, 0.95, 'seed_vtable_thunk')
            accepted += 1
        return accepted, rejected

    def run(self):
        started = time.monotonic()
        metadata = self._find_analysis_json()
        cache = AnalysisCache(self.output_dir)
        options = (self.text_only, self.extra_sections, self.seed_functions, self.observed_seeds)
        self.cache_hit = not self.force and cache.is_valid(self.xbe_path, metadata, *options)
        if self.cache_hit:
            print('Analysis cache verified: inputs, options, source and output contents match')
            if self.stats_only or self.verbose:
                self._load_and_print_cached_stats()
            return True
        self.image = load_image(self.xbe_path, metadata)
        selected = self._get_target_sections()
        self.labels = LabelManager()
        populate_entry_point(self.labels, self.image)
        populate_kernel_labels(self.labels, self.image)
        self.strings = extract_strings(self.image)
        populate_string_labels(self.labels, self.strings)
        self.engine = CodeRecovery(self.image)
        for section in selected:
            count = self.engine.linear_sweep(section)
            if self.verbose:
                print(f'{section.name}: {count:,} instructions')
        self.engine.resync_jump_tables()
        self.xrefs = build_xrefs(self.engine, self.image)
        self.func_detector = FunctionDetector(self.engine, self.image, self.xrefs, self.labels)
        accepted, rejected = self._seed()
        if self.verbose and self.seed_functions:
            print(f'Seeds: {accepted} accepted, {rejected} rejected')
        self.func_detector.detect_all(selected)
        reachable = self.engine.recursive_descent([self.image.entry_point, *self.func_detector.functions],
                        [(section.virtual_addr, section.virtual_addr + section.virtual_size) for section in selected])
        if self.stats_only or self.verbose:
            print_stats(self.engine, self.func_detector, self.xrefs, self.labels, self.strings, self.image)
            print(f'{len(reachable):,} instructions reachable over cached direct edges')
        if not self.stats_only:
            # A failed write must not leave a valid receipt for mixed outputs.
            cache.invalidate()
            writer = OutputWriter(self.output_dir, self.engine, self.func_detector, self.xrefs,
                                  self.labels, self.image, self.strings)
            writer.write_all(selected, self.verbose)
            cache.save(self.xbe_path, metadata, self.text_only, time.monotonic() - started,
                       self.extra_sections, self.seed_functions, self.observed_seeds)
        print(f'Analysis complete in {time.monotonic() - started:.2f}s')
        return True

    def _load_and_print_cached_stats(self):
        summary = json.loads((Path(self.output_dir) / 'summary.json').read_text(encoding='utf-8'))
        print(f"Cached analysis: {summary['total_functions']:,} functions, "
              f"{summary['total_instructions']:,} instructions, {summary['total_xrefs']:,} references")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('xbe_path')
    parser.add_argument('-o', '--output', dest='output_dir')
    parser.add_argument('--analysis-json')
    for option in ('text-only', 'stats-only', 'force'):
        parser.add_argument('--' + option, action='store_true')
    parser.add_argument('-v', '--verbose', action='store_true')
    parser.add_argument('--extra-sections', help='Comma-separated section names to analyze as code')
    parser.add_argument('--seed-functions', action='append', default=[], metavar='JSON')
    args = parser.parse_args(argv)
    try:
        observed, seeds = set(), set()
        for path in args.seed_functions:
            seeds.update(_load_seed_functions(path, observed))
        extra = [name.strip() for name in (args.extra_sections or '').split(',') if name.strip()]
        run = Disassembler(args.xbe_path, args.analysis_json, args.output_dir, args.text_only,
                          args.stats_only, args.verbose, args.force, extra, seeds, observed)
        return 0 if run.run() else 1
    except KeyboardInterrupt:
        return 130
    except (OSError, ValueError) as error:
        print(f'Analysis failed: {error}', file=sys.stderr)
        return 1


if __name__ == '__main__':
    raise SystemExit(main())
