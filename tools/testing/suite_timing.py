"""Advisory pytest timing history; never select tests or change their exit status."""
from collections import Counter
from datetime import datetime, timezone
import hashlib
from importlib.metadata import PackageNotFoundError, version
import json
import os
from pathlib import Path
import platform
import re
import statistics
import subprocess
import tempfile
import time

import pytest

SCHEMA = 1
SAMPLES = 3
THRESHOLD = 1.25
HISTORY_LIMIT = 60


def canonical(nodeid):
    return re.sub(r'@tsfp-[0-9a-f]{24}$', '', nodeid)


def environment(config):
    def package(name):
        try:
            return version(name)
        except PackageNotFoundError:
            return 'absent'
    # Hosted runners have disposable hostnames. Keep their OS image family and
    # hardware separate from local machines and self-hosted runners.
    host = (os.environ.get('ImageOS', 'unknown-image')
            if os.environ.get('RUNNER_ENVIRONMENT') == 'github-hosted'
            else hashlib.sha256(platform.node().encode()).hexdigest()[:16])
    return dict(system=platform.system(), architecture=platform.machine(),
                processor=platform.processor(), cpus=os.cpu_count(), host=host,
                runner=os.environ.get('RUNNER_ENVIRONMENT', 'local'),
                python=platform.python_version(), pytest=package('pytest'),
                xdist=package('pytest-xdist'),
                workers=config.getoption('numprocesses', default=0) or 1,
                distribution=config.getoption('dist', default='no'))


def coverage(reference, current):
    return dict(missing=sorted(set(reference)-set(current)),
                added=sorted(set(current)-set(reference)),
                newly_skipped=sorted(name for name, outcome in current.items()
                                     if outcome != 'passed' and reference.get(name) != outcome))


def update(state, run):
    """Fixed baseline; unhealthy/partial runs never train it or extend a streak."""
    cases = run['cases']
    guard = state.get('guard', {})
    changes = coverage(guard, cases) if guard else dict(missing=[], added=[], newly_skipped=[])
    healthy = run['complete'] and run['exitstatus'] == 0 and all(
        result in ('passed', 'skipped') for result in cases.values())
    safe = healthy and not changes['missing'] and not changes['newly_skipped']
    messages = []
    warnings = []
    if not run['complete']:
        warnings.append('Partial or filtered run: excluded from baseline and speed comparison.')
    if changes['missing'] or changes['newly_skipped']:
        warnings.append(f"Coverage changed: {len(changes['missing'])} missing tests, "
                        f"{len(changes['newly_skipped'])} newly skipped/non-passing tests. Review last-run.json.")
    baseline = state.get('baseline')
    ratio = None
    if baseline is None and safe:
        samples = state.setdefault('samples', [])
        if samples and samples[-1]['cases'] != cases:
            samples.clear()
            messages.append('Collection/outcomes changed during calibration; starting three comparable samples.')
        samples.append(dict(seconds=run['seconds'], cases=cases, revision=run['revision']))
        if len(samples) == SAMPLES:
            state['baseline'] = baseline = dict(seconds=statistics.median(s['seconds'] for s in samples),
                                               revisions=[s['revision'] for s in samples], samples=SAMPLES)
            state['guard'] = dict(cases)
            state['samples'] = []
            messages.append(f"Baseline established: {baseline['seconds']:.1f}s median of three clean runs.")
        else:
            messages.append(f'Baseline learning: {len(samples)}/{SAMPLES} clean comparable runs.')
    elif baseline is not None and safe:
        ratio = run['seconds']/baseline['seconds']
        state['guard'].update(cases)
        state['slow_streak'] = state.get('slow_streak', 0)+1 if ratio > THRESHOLD else 0
        messages.append(f"{run['seconds']:.1f}s; baseline {baseline['seconds']:.1f}s; "
                        f"{(ratio-1)*100:+.1f}%; slow streak {state['slow_streak']}/{SAMPLES}.")
        if state['slow_streak'] >= SAMPLES:
            warnings.append('Sustained suite slowdown: three or more consecutive clean runs exceed the fixed baseline by 25%. Review the slowest tests.')
    if not safe:
        state['slow_streak'] = 0
        messages.append('Unsuccessful or coverage-changing run retained without training the baseline.')
    summary = {key: run[key] for key in ('timestamp', 'seconds', 'exitstatus', 'complete', 'revision', 'dirty', 'counts', 'slowest')}
    summary.update(ratio=ratio, coverage={key: len(value) for key, value in changes.items()}, warnings=warnings)
    state.setdefault('history', []).append(summary)
    state['history'] = state['history'][-HISTORY_LIMIT:]
    return dict(messages=messages, warnings=warnings, coverage=changes, baseline=baseline, ratio=ratio)


def write_json(path, data):
    if path.is_symlink():
        raise ValueError('Timing output must not be a symbolic link')
    with tempfile.NamedTemporaryFile(mode='w', encoding='utf-8', dir=path.parent,
                                     prefix='.timing-', suffix='.tmp', delete=False) as stream:
        temporary = Path(stream.name)
        json.dump(data, stream, allow_nan=False, sort_keys=True)
        stream.write('\n')
    try:
        os.replace(temporary, path)
    finally:
        temporary.unlink(missing_ok=True)


def store_run(root, profile, identity, run):
    key = hashlib.sha256(json.dumps(identity, sort_keys=True).encode()).hexdigest()[:20]
    folder = root/profile/key
    for path in (root, root/profile, folder):
        if path.is_symlink():
            raise ValueError('Timing history directory must not be a symbolic link')
        path.mkdir(parents=True, exist_ok=True)
    # Advisory failure on an occupied/stale lock; never overwrite another writer.
    lock = folder/'state.lock'
    with lock.open('x'):
        pass
    try:
        path = folder/'state.json'
        if path.is_symlink():
            raise ValueError('Timing state must not be a symbolic link')
        state = json.loads(path.read_text(encoding='utf-8'),
                           parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value))) if path.exists() else dict(version=SCHEMA, identity=identity)
        if state.get('version') != SCHEMA or state.get('identity') != identity:
            raise ValueError('Timing state version or environment mismatch; retained for review')
        baseline = state.get('baseline')
        if baseline is not None and (not isinstance(baseline['seconds'], (int, float)) or baseline['seconds'] <= 0):
            raise ValueError('Invalid timing baseline; retained for review')
        result = update(state, run)
        write_json(folder/'last-run.json', dict(run, assessment=result, identity=identity))
        write_json(path, state)
        return folder, result
    finally:
        lock.unlink()


def pytest_addoption(parser):
    group = parser.getgroup('suite timing')
    group.addoption('--suite-profile', default=None, help='Opt in to advisory suite timing (e.g. release-full).')
    group.addoption('--suite-history-root', default=None, help='History directory; defaults to build/test-performance.')


def pytest_configure(config):
    profile = config.getoption('suite_profile')
    if profile and not hasattr(config, 'workerinput'):
        if not re.fullmatch(r'[a-z0-9][a-z0-9-]{0,63}', profile):
            raise pytest.UsageError('suite-profile must contain lowercase letters, digits and hyphens')
        config.pluginmanager.register(Monitor(config, profile), 'suite-timing-monitor')


class Monitor:
    def __init__(self, config, profile):
        self.config, self.profile = config, profile
        self.started = time.perf_counter()
        self.collected = set()
        self.outcomes = {}
        self.durations = Counter()
        self.subtests = Counter()
        self.deselected = 0
        self.result = None

    def pytest_collection_finish(self, session):
        self.collected.update(canonical(item.nodeid) for item in session.items)

    @pytest.hookimpl(optionalhook=True)
    def pytest_xdist_node_collection_finished(self, node, ids):
        self.collected.update(canonical(name) for name in ids)

    def pytest_deselected(self, items):
        self.deselected += len(items)

    def pytest_runtest_logreport(self, report):
        if hasattr(report, 'context'):  # Subtests share the parent's node ID.
            self.subtests[report.outcome] += 1
            return
        name = canonical(report.nodeid)
        self.durations[name] += report.duration
        outcome = report.outcome
        if hasattr(report, 'wasxfail'):
            outcome = 'xfailed' if report.skipped else 'xpassed'
        if report.failed:
            outcome = 'failed' if report.when == 'call' else 'error'
        if report.when == 'call' or outcome not in ('passed',):
            if self.outcomes.get(name) not in ('failed', 'error'):
                self.outcomes[name] = outcome

    def pytest_sessionfinish(self, session, exitstatus):
        seconds = time.perf_counter()-self.started
        root = Path(__file__).resolve().parents[2]
        def git(*args):
            return subprocess.check_output(['git', '-C', str(root), *args], stderr=subprocess.DEVNULL, timeout=5).decode().strip()
        try:
            revision, dirty = git('rev-parse', 'HEAD'), bool(git('status', '--porcelain', '--untracked-files=normal'))
        except (OSError, subprocess.SubprocessError):
            revision, dirty = 'unknown', True
        filtered = self.deselected or any(self.config.getoption(name, default=False)
                                         for name in ('keyword', 'markexpr', 'lf', 'stepwise', 'deselect', 'collectonly', 'ignore', 'ignore_glob'))
        complete = bool(self.collected) and self.collected == set(self.outcomes) and not filtered
        run = dict(timestamp=datetime.now(timezone.utc).isoformat(), seconds=seconds,
                   exitstatus=int(exitstatus), complete=bool(complete), revision=revision, dirty=dirty,
                   collected=len(self.collected), cases=self.outcomes, counts=dict(Counter(self.outcomes.values())),
                   subtests=dict(self.subtests), slowest=self.durations.most_common(20))
        history = Path(self.config.getoption('suite_history_root') or root/'build/test-performance')
        try:
            folder, result = store_run(history, self.profile, environment(self.config), run)
            self.result = result
            result['messages'].append(f'History: {folder}')
        except Exception as error:
            # This is an observability boundary, never a replacement test gate.
            # Preserve pytest's result even for malformed optional history.
            self.result = dict(messages=[], warnings=[f'Timing monitor unavailable ({type(error).__name__}); test result unchanged. Check timing history/lock.'])

    def pytest_terminal_summary(self, terminalreporter):
        if self.result:
            terminalreporter.write_sep('-', 'Suite performance (advisory)')
            for message in self.result['messages']:
                terminalreporter.write_line(message)
            for message in self.result['warnings']:
                terminalreporter.write_line('WARNING: '+message, yellow=True)
                if os.environ.get('GITHUB_ACTIONS') == 'true':
                    terminalreporter.write_line('::warning title=Suite performance::'+message)
