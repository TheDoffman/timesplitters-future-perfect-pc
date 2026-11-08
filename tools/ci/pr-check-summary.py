"""Advisory overview of this workflow attempt; never decides merge acceptance."""
from collections import defaultdict
from datetime import datetime
import html
import json
import os
from pathlib import Path
import re
from urllib.request import Request, urlopen


def render(jobs, run_url):
    groups = defaultdict(list)
    for job in jobs:
        if job['name'] == 'PR validation':
            continue
        name = re.sub(r' · [1-3]/3$', '', job['name'])
        groups[name].append(job)
    rows, skipped = [], []
    for name, members in sorted(groups.items()):
        conclusions = [job.get('conclusion') for job in members]
        label = html.escape(name).replace('|', '&#124;').replace('\n', ' ')
        if all(value == 'skipped' for value in conclusions):
            skipped.append(label)
            continue
        passed = conclusions.count('success')
        result = 'Passed' if passed == len(members) else 'Needs attention'
        starts = [datetime.fromisoformat(j['started_at'].replace('Z', '+00:00'))
                  for j in members if j.get('started_at')]
        ends = [datetime.fromisoformat(j['completed_at'].replace('Z', '+00:00'))
                for j in members if j.get('completed_at')]
        duration = '—'
        if starts and len(ends) == len(members):
            seconds = max(0, int((max(ends) - min(starts)).total_seconds()))
            duration = f'{seconds // 60}m {seconds % 60:02d}s'
        # Construct links from numeric IDs, never interpolate arbitrary job URLs.
        failures = [j for j in members if j.get('conclusion') not in ('success', 'skipped')]
        links = ', '.join(f'[Job {i + 1}]({run_url}/job/{int(j["id"])})'
                          for i, j in enumerate(failures)) or f'[Details]({run_url})'
        rows.append(f'| {label} | {result} ({passed}/{len(members)}) | {duration} | {links} |')
    output = ['## PR checks', '', '[Open workflow run](' + run_url + ')', '',
              '| Check group | Result | Duration | Logs |',
              '| --- | --- | --- | --- |', *rows, '',
              'Durations span the first start to the last finish in each group; '
              'parallel job times are not added together. PR validation determines merge acceptance.']
    if skipped:
        output += ['', '<details><summary>Skipped checks</summary>', '',
                   *['- ' + name for name in skipped], '', '</details>']
    return '\n'.join(output) + '\n'


def main():
    repository = os.environ['GITHUB_REPOSITORY']
    run_id = int(os.environ['GITHUB_RUN_ID'])
    attempt = int(os.environ['GITHUB_RUN_ATTEMPT'])
    api = os.environ.get('GITHUB_API_URL', 'https://api.github.com').rstrip('/')
    server = os.environ.get('GITHUB_SERVER_URL', 'https://github.com').rstrip('/')
    jobs = []
    for page in range(1, 101):
        url = f'{api}/repos/{repository}/actions/runs/{run_id}/attempts/{attempt}/jobs?per_page=100&page={page}'
        request = Request(url, headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
                                       'Accept': 'application/vnd.github+json',
                                       'X-GitHub-Api-Version': '2022-11-28'})
        with urlopen(request, timeout=15) as response:
            batch = json.load(response)['jobs']
        jobs.extend(batch)
        if len(batch) < 100:
            break
    else:
        raise RuntimeError('Job summary exceeded pagination limit')
    run_url = f'{server}/{repository}/actions/runs/{run_id}'
    with Path(os.environ['GITHUB_STEP_SUMMARY']).open('a', encoding='utf-8') as output:
        output.write(render(jobs, run_url))


if __name__ == '__main__':
    main()
