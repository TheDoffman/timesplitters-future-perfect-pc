# Background texture-update checks

TextureUpdateCheck in TextureDownload.cs owns one launcher session's read-only
manifest and installed-file comparison. StartIfOptedIn respects the existing
texture-opt-in marker and runs automatically once per session after startup
recovery. CheckAsync also supports explicit retries and shares an in-flight task.
Fetching and hashing run off the UI thread; texture checks never set launcher
busy or show the settings operation overlay. Fetching retains its30-second
network deadline and the overall check has a two-minute cancellation deadline.

Each WPF/Avalonia window owns and disposes one monitor. Ui.Post marshals its
Changed event to the host UI thread; a closed-window guard drops queued updates.
The shared Launcher.xaml footer contains TextureUpdateStatus. It reports checking,
up-to-date, available or unavailable without replacing game/update operation
messages. The tooltip gives details or points to Graphics / Enhanced textures.
Preview and isolated session modes do not start automatic network requests.

SettingsPage receives the same monitor and applies cached results when opened;
opening Settings no longer starts a request. Manual Check is also nonblocking.
Only texture actions wait during a check; staged settings remain editable.
The preference now says checks happen when the launcher opens, and enabling it
can start the current session's check. Opt-out remains the fresh-install default.
Downloads and explicit file verification keep their existing confirmation and
operation flows. Successful installation/verification refreshes the shared result.
No pack is installed automatically, and opting into checks does not enable textures.

Shared behavioral tests inject a pending fetch and read-only scan: opt-out,
nonblocking editing, coalescing, one automatic check, cached settings results,
offline errors, install status and cancellation on close. Windows full launcher
regressions and Linux cross-build pass; actual Mac/Linux startup/visual acceptance
is pending. A local footer preview uses sample check state without network traffic.
