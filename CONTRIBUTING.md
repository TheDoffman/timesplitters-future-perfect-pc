# Contributing

Thanks for helping improve the TimeSplitters: Future Perfect PC project!
Testing, clear reports, documentation and helping other players all count.
Please follow the [code of conduct](CODE_OF_CONDUCT.md).

## Pick the right place

- **Something broken?** Search [existing issues](https://github.com/TheDoffman/timesplitters-future-perfect-pc/issues),
  then use [Report a problem](https://github.com/TheDoffman/timesplitters-future-perfect-pc/issues/new?template=01-bug-report.yml).
  Enter your installed game version, launcher version and platform, then explain
  what happened and how to reproduce it. Screenshots and relevant logs help.
- **An improvement?** Check [existing Ideas](https://github.com/TheDoffman/timesplitters-future-perfect-pc/discussions/categories/ideas), then [start an Ideas discussion](https://github.com/TheDoffman/timesplitters-future-perfect-pc/discussions/new?category=ideas). Share one suggestion per discussion.
- **Questions or an idea to talk through?** Use [Discussions](https://github.com/TheDoffman/timesplitters-future-perfect-pc/discussions).
- **A security vulnerability?** Follow [SECURITY.md](SECURITY.md) and report it privately.

Remove personal information, tokens and private join codes from screenshots
and logs. Never upload game files, ISOs, generated game code or personal saves.

## Send a change

See the [repository layout](docs/development/repository-layout.md) for file locations,
naming conventions and the distinction between source and local build data.

1. For a large change, discuss the approach in an issue or Discussion first.
2. Fork the repository and make a focused branch from the latest `main`.
3. For code changes, read [AGENTS.md](AGENTS.md), the shared project rules, and
   the [repository map](docs/agent-context/README.md). Update each affected
   subsystem guide in the same change; CI checks this maintenance requirement.
   Use the [build guide](docs/development/building.md) or [Mac guide](docs/players/macos.md) as needed.
4. Keep the change focused and run the checks relevant to it. Explain what you
   tested, on which platform, and what remains untested. Use disposable test
   saves and settings; leave personal files alone. Documentation-only changes
   do not need a game rebuild.
5. Open a pull request using the template. Explain the problem, how your change
   addresses it and any remaining limits. Link an issue it resolves with
   `Closes #123` where appropriate. Mention Mac impact as the template requests.

Keep fixes in shared source or reproducible patches; do not edit generated
game C. Preserve copyright notices and third-party credits. Only submit work
you have permission to contribute, under the project's [license](README.md#license)
or the applicable existing file license.

Maintainers review changes before merging. This is a community project, so
review and release timing can vary.

CI reports on every PR, including documentation changes. All required checks
must pass and the branch must include the latest `main` before merging. Use
GitHub's **Update branch** button (or merge `main` into your branch) when needed.
Maintainers can choose **Enable auto-merge** on an individual PR to merge it
after these requirements are met. See [CI and branch protection](docs/development/ci.md).
