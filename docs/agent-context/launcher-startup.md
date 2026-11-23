## Cold startup network ordering (2026-11-23)

Both WPF Launcher.Loaded and Mac/Linux NativeWindow.Opened now await the shared
TextureUpdateCheck.StartAfter(Check()) before continuing startup. StartAfter
waits for the initial game-release check, then starts the optional texture check
without waiting for its download/scan. This avoids simultaneous cold HTTP
requests after updates, when --skip-self-update bypasses the bootstrap request.
The pinned macOS .NET8 runtime reproduced a fatal access violation in HTTP/socket
code with the previous ordering. This is a launcher sequencing mitigation, not
an upstream runtime fix. Texture opt-in, background scanning, cancellation and
manual checks remain; no settings/dependency change. Shared code retains C#5 syntax.
The shared UI suite covers deferred startup, nonblocking completion, opt-out and
closing while the initial check is pending. See STATUS for native reproduction
and bounded restart evidence; Windows/Linux execution remains pending. Existing launcher-build.yml runs the
shared suite. Cold-start probes must enable a disposable texture opt-in and
--skip-self-update; the ordinary rendered package preview omits network work.

