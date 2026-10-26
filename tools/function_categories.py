"""Source-string and call-graph evidence for the project function catalog."""
from collections import Counter, defaultdict
from pathlib import PurePosixPath
import re

from function_evidence import address


def source_identifiers(strings):
    """Extract embedded RenderWare source identifiers without a title layout."""
    result = []
    for record in strings:
        text = record['string'].replace('\\', '/')
        match = re.search(r'(?i)(?:^|/)RenderWare/[^\s]+?/rwsdk/([^#$\r\n]+)', text)
        if match is None:
            continue
        path = PurePosixPath(match.group(1).strip())
        if path.is_absolute() or '..' in path.parts or path.suffix.lower() not in ('.c', '.cpp', '.h'):
            continue
        directories = list(path.parts[:-1])
        if directories and directories[0].lower() == 'src':
            directories.pop(0)
        category_parts = [re.sub('[^a-z0-9]+', '_', part.lower()).strip('_')
                          for part in directories if part.lower() != 'p2']
        category = 'rw_' + ('_'.join(part for part in category_parts if part) or 'core')
        result.append(dict(address=address(record['address']), path=str(path),
                           filename=path.name, category=category))
    return sorted(result, key=lambda record: (record['address'], record['path']))


def direct_source_labels(identifiers, references):
    """Retain a deterministic source attribution and all corroborating paths."""
    evidence = defaultdict(list)
    for source in identifiers:
        for function in references.get(source['address'], ()):
            evidence[function].append(source)
    result = {}
    for function, sources in sorted(evidence.items()):
        categories = Counter(source['category'] for source in sources)
        category = sorted(categories, key=lambda item: (-categories[item], item))[0]
        source = min((item for item in sources if item['category'] == category),
                     key=lambda item: (item['path'], item['address']))
        result[function] = dict(category=category, confidence=0.95, method='source_identifier',
                                module=source['filename'], source_file=source['path'],
                                evidence_paths=sorted({item['path'] for item in sources}))
    return result


def propagate_categories(functions, initial, max_rounds=8):
    """Infer unanimous neighbor categories in bounded, order-independent rounds.

    At least two distinct callers or callees must agree, and every neighbor in
    that direction must already be classified. Confidence decreases from the
    weakest witness. Unseeded cycles and conflicting neighborhoods stay unknown.
    """
    known = {address(record['start']) for record in functions}
    outgoing, incoming = defaultdict(set), defaultdict(set)
    for record in functions:
        source = address(record['start'])
        for target in record.get('calls_to', ()):
            target = address(target)
            if target in known and target != source:
                outgoing[source].add(target)
                incoming[target].add(source)
    labels = {key: dict(value) for key, value in initial.items() if key in known}
    inferred = {}
    for _ in range(max_rounds):
        pending = {}
        for function in sorted(known - labels.keys()):
            proposals = []
            for direction, neighbors in (('callers', incoming[function]), ('callees', outgoing[function])):
                if len(neighbors) < 2 or not neighbors <= labels.keys():
                    continue
                categories = {labels[neighbor]['category'] for neighbor in neighbors}
                if len(categories) == 1:
                    confidence = min(labels[neighbor]['confidence'] for neighbor in neighbors) * 0.8
                    proposals.append(dict(category=categories.pop(), confidence=confidence,
                                          method='graph_' + direction, witnesses=sorted(neighbors)))
            if proposals and len({proposal['category'] for proposal in proposals}) == 1:
                pending[function] = max(proposals, key=lambda proposal: proposal['confidence'])
        if not pending:
            break
        labels.update(pending)
        inferred.update(pending)
    return inferred


def contextual_labels(image, functions, strings, references):
    """Rank explicit words and calls into named SDK sections as weak hints.

    This vocabulary is deliberately small. Tied subsystem evidence is retained
    as ambiguity, rather than resolved by dictionary order. Neither a word nor
    a library call proves the caller's purpose or its library provenance.
    """
    words = {name: set(tokens.split()) for name, tokens in {
        'audio': 'audio sound music reverb', 'render': 'render shader texture vertex',
        'network': 'network socket multiplayer matchmaking', 'physics': 'physics collision gravity',
        'camera': 'camera', 'ui': 'menu hud font', 'io': 'filesystem savegame partition',
        'input': 'controller gamepad joystick', 'video': 'movie video',
    }.items()}
    sections = {'D3D': 'render', 'XGRPH': 'render', 'DSOUND': 'audio',
                'DOLBY': 'audio', 'XNET': 'network', 'XONLINE': 'network', 'XMV': 'video'}
    evidence = defaultdict(lambda: defaultdict(set))
    for record in strings:
        tokens = set(re.findall(r'[a-z]+', record['string'].lower()))
        location = address(record['address'])
        for category, vocabulary in words.items():
            if tokens & vocabulary:
                for function in references.get(location, ()):
                    evidence[function][category].add(('string', location))
    for record in functions:
        source = address(record['start'])
        for target in {address(value) for value in record.get('calls_to', ())}:
            section = image.get_section_at_va(target)
            category = sections.get(section.name.upper()) if section and section.executable else None
            if category:
                evidence[source][category].add(('call', target))
    result = {}
    for function, alternatives in sorted(evidence.items()):
        ranks = sorted(alternatives, key=lambda name: (-len(alternatives[name]), name))
        first = ranks[0]
        if len(ranks) > 1 and len(alternatives[first]) == len(alternatives[ranks[1]]):
            continue
        result[function] = dict(category='game_' + first, confidence=0.65,
            method='context_evidence', evidence={name: [dict(kind=kind, address=location)
                for kind, location in sorted(alternatives[name])] for name in sorted(alternatives)})
    return result
