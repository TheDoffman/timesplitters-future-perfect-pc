using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;

namespace TimeSplitters.Launcher
{
    public static class LauncherLog
    {
        static readonly object gate = new object();
        public static void Write(string root, string text)
        {
            lock (gate)
            {
                try
                {
                    string path = Core.PathIn(root, ".launcher/launcher.log");
                    Directory.CreateDirectory(Path.GetDirectoryName(path));
                    if (File.Exists(path) && new FileInfo(path).Length > 1024 * 1024) File.WriteAllText(path, "");
                    File.AppendAllText(path, DateTime.UtcNow.ToString("u") + " " + text + Environment.NewLine);
                }
                catch (IOException) { }
                catch (UnauthorizedAccessException) { }
            }
        }
    }
    public static class SelfUpdate
    {
        public static async Task<Release> CheckWithTimeout(ReleaseClient client, bool preview, TimeSpan limit, CancellationToken token)
        {
            using (var request = CancellationTokenSource.CreateLinkedTokenSource(token))
            {
                var pending = client.CheckLauncher(preview, request.Token);
                if (await Task.WhenAny(pending, Task.Delay(limit, token)).ConfigureAwait(false) != pending)
                {
                    request.Cancel();
                    // Observe eventual failures even if an OS network call ignores cancellation.
                    ObserveFailure(pending);
                    token.ThrowIfCancellationRequested();
                    throw new TimeoutException("Launcher update check timed out; opening the installed launcher.");
                }
                return await pending.ConfigureAwait(false);
            }
        }
        static void ObserveFailure(Task pending)
        { pending.ContinueWith(task => { var error = task.Exception; }, TaskContinuationOptions.OnlyOnFaulted); }
        public static bool PreviewChannel(string root)
        {
            string path = Core.PathIn(root, ".launcher/preferences.json");
            try { return File.Exists(path) && Json.Flag(Json.Object(File.ReadAllText(path)), "launcher_preview"); }
            catch (Exception ex) { LauncherLog.Write(root, "Launcher preferences: " + ex.Message); return false; }
        }
        public static string InstalledVersion(string root)
        {
            string path = Core.PathIn(root, Core.SelfManifest);
            if (!File.Exists(path)) return "";
            try
            {
                string version = Json.String(Json.Object(File.ReadAllText(path)), "version");
                GameVersion.Parse(version); return version;
            }
            catch (Exception ex) { LauncherLog.Write(root, "Launcher manifest: " + ex.Message); return ""; }
        }
        internal static Dictionary<string, string> Manifest(string text, string expected)
        {
            var metadata = Json.Object(text);
            Core.Require(Json.String(metadata, "schema") == "1" && Json.String(metadata, "version") == expected, "Launcher package version or schema mismatch.");
            GameVersion.Parse(expected);
            object hashes;
            Core.Require(metadata.TryGetValue("files", out hashes) && hashes is Dictionary<string, object>, "Launcher file manifest is missing.");
            var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (var pair in (Dictionary<string, object>)hashes)
            {
                Core.Name(pair.Key);
                Core.Require((pair.Key == Core.LauncherName || pair.Key == Core.AppPath || LauncherPlatform.LauncherResource(pair.Key)) && !result.ContainsKey(pair.Key), "Unexpected launcher package file.");
                string hash = Convert.ToString(pair.Value);
                Core.Require(Regex.IsMatch(hash, "^[0-9a-fA-F]{64}$"), "Invalid launcher file checksum.");
                result.Add(pair.Key, hash);
            }
            Core.Require(result.Count >= 2 && result.Count <= LauncherPlatform.LauncherFileLimit && result.ContainsKey(Core.LauncherName) && result.ContainsKey(Core.AppPath), "Launcher package is incomplete.");
            return result;
        }
        public static bool Complete(string root)
        {
            try
            {
                string version = InstalledVersion(root);
                if (version.Length == 0) return false;
                var files = Manifest(File.ReadAllText(Core.PathIn(root, Core.SelfManifest)), version);
                return files.All(pair => File.Exists(Core.PathIn(root, pair.Key)) && Core.Hash(Core.PathIn(root, pair.Key)).Equals(pair.Value, StringComparison.OrdinalIgnoreCase));
            }
            catch (Exception ex) { LauncherLog.Write(root, "Launcher file validation: " + ex.Message); return false; }
        }
        // Called with the installation mutex held, after the main window loads.
        // Until then the previous launcher remains available for recovery.
        public static void CleanupAfterStartup(string root)
        {
            try
            {
                if (Directory.Exists(Core.PathIn(root, ".launcher/self-transaction")) || !Complete(root)) return;
                TryCleanup(root, ".launcher/self-rollback", true);
                string pending = Core.PathIn(root, ".launcher/self-pending");
                if (Directory.Exists(pending))
                {
                    string marker = Core.PathIn(pending, "release.json");
                    // An uncommitted download can be discarded. Keep a newer
                    // staged update (or unreadable marker) for its helper/recovery.
                    bool obsolete = !File.Exists(marker);
                    if (!obsolete)
                    {
                        try { obsolete = GameVersion.Parse(Json.String(Json.Object(File.ReadAllText(marker)), "version"))
                            .CompareTo(GameVersion.Parse(InstalledVersion(root))) <= 0; }
                        catch (Exception ex) { LauncherLog.Write(root, "Launcher staging cleanup deferred: " + ex.Message); }
                    }
                    if (obsolete) TryCleanup(root, ".launcher/self-pending", true);
                }
                // A prepared update still owns its helper, even while waiting
                // for the installation mutex. Windows also protects running EXEs.
                if (!Directory.Exists(pending)) TryCleanup(root, ".launcher/self-helper.exe", false);
                if (!Directory.Exists(Core.PathIn(root, ".launcher/transaction")) &&
                    !Directory.Exists(Core.PathIn(root, ".launcher/pending")))
                    TryCleanup(root, ".launcher/update-helper.exe", false);
            }
            catch (Exception ex) { LauncherLog.Write(root, "Launcher cleanup deferred: " + ex.Message); }
        }
        static void TryCleanup(string root, string relative, bool directory)
        {
            try
            {
                if (directory) Core.RemoveOwned(root, relative);
                else
                {
                    string path = Core.PathIn(root, relative);
                    if (File.Exists(path)) File.Delete(path);
                }
            }
            catch (Exception ex) { LauncherLog.Write(root, "Launcher cleanup deferred for " + relative + ": " + ex.Message); }
        }
        public static List<Change> Unpack(string archive, string destination, string expected, CancellationToken token = default(CancellationToken))
        {
            token.ThrowIfCancellationRequested();
            Core.Require(!Directory.Exists(destination), "Launcher staging already exists.");
            using (var zip = ZipFile.OpenRead(archive))
            {
                Core.Require(zip.Entries.Count > 0 && zip.Entries.Count <= LauncherPlatform.LauncherFileLimit, "Unexpected launcher archive layout.");
                var manifestEntries = zip.Entries.Where(e => e.FullName == Core.SelfManifest).ToArray();
                Core.Require(manifestEntries.Length == 1 && manifestEntries[0].Length <= LauncherPlatform.LauncherManifestLimit, "Launcher manifest is missing or too large.");
                string text;
                using (var reader = new StreamReader(manifestEntries[0].Open())) text = reader.ReadToEnd();
                var hashes = Manifest(text, expected);
                var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                long total = 0;
                foreach (var entry in zip.Entries)
                {
                    Core.Name(entry.FullName.TrimEnd('/'));
                    Core.Require(((entry.ExternalAttributes >> 16) & 0xF000) != 0xA000 && (entry.ExternalAttributes & 0x400) == 0, "Linked launcher archive entry.");
                    if (entry.FullName.EndsWith("/", StringComparison.Ordinal)) continue;
                    Core.Require(Core.LauncherFile(entry.FullName) && names.Add(entry.FullName), "Unexpected or duplicate launcher archive file.");
                    total += entry.Length;
                    Core.Require(entry.Length <= LauncherPlatform.LauncherExpandedLimit && total <= LauncherPlatform.LauncherExpandedLimit, "Expanded launcher package is too large.");
                }
                Core.Require(names.Count == hashes.Count + 1, "Launcher package files are missing.");
                Core.CheckSpace(Path.GetDirectoryName(archive), total * 2 + 16 * 1024 * 1024);
                Directory.CreateDirectory(destination);
                foreach (var entry in zip.Entries.Where(e => !e.FullName.EndsWith("/", StringComparison.Ordinal)))
                {
                    token.ThrowIfCancellationRequested();
                    string target = Core.PathIn(destination, entry.FullName);
                    Directory.CreateDirectory(Path.GetDirectoryName(target));
                    using (var input = entry.Open()) using (var output = new FileStream(target, FileMode.CreateNew)) {
                        var buffer = new byte[65536]; int read;
                        while ((read = input.Read(buffer,0,buffer.Length)) > 0) { token.ThrowIfCancellationRequested(); output.Write(buffer,0,read); }
                    }
                    if (entry.FullName != Core.SelfManifest) Core.Require(Core.Hash(target).Equals(hashes[entry.FullName], StringComparison.OrdinalIgnoreCase), "Launcher file checksum mismatch.");
                }
                return ValidatePayload(destination, expected);
            }
        }
        static List<Change> ValidatePayload(string payload, string expected)
        {
            var hashes = Manifest(File.ReadAllText(Core.PathIn(payload, Core.SelfManifest)), expected);
            var changes = new List<Change>();
            foreach (var pair in hashes)
            {
                string path = Core.PathIn(payload, pair.Key);
                Core.Require(Core.Hash(path).Equals(pair.Value, StringComparison.OrdinalIgnoreCase), "Staged launcher file changed: " + pair.Key);
                changes.Add(new Change { Name = pair.Key, Source = path });
            }
            changes.Add(new Change { Name = Core.SelfManifest, Source = Core.PathIn(payload, Core.SelfManifest) });
            return changes;
        }
        public static async Task<bool> Prepare(string root, Release release, ReleaseClient client, IProgress<double> progress, CancellationToken token, bool repair = false)
        {
            if (release == null) return false;
            string installed = InstalledVersion(root);
            Core.Require(!repair || release.Tag == LauncherVersion.Value, "Repair requires the currently running launcher release.");
            if (!repair && installed.Length > 0 && Complete(root) && GameVersion.Parse(release.Tag).CompareTo(GameVersion.Parse(installed)) <= 0) return false;
            if (installed.Length > 0 && GameVersion.Parse(release.Tag).CompareTo(GameVersion.Parse(installed)) < 0) return false;
            Core.Require(!Core.GameRunning(), "Close the game before installing a launcher update.");
            Core.CheckSpace(root, release.Size * 4 + 32 * 1024 * 1024);
            Core.Require(!repair || !Directory.Exists(Core.PathIn(root, ".launcher/self-pending")), "A launcher update is already staged. Restart the launcher before repairing.");
            Core.RemoveOwned(root, ".launcher/self-pending");
            string pending = Core.PathIn(root, ".launcher/self-pending"); Directory.CreateDirectory(pending);
            try
            {
                string archive = Path.Combine(pending, "launcher.zip");
                await client.Download(release, archive, progress, token, Core.PathIn(root,".launcher/downloads/launcher")).ConfigureAwait(false);
                token.ThrowIfCancellationRequested();
                await Task.Run(() => Unpack(archive, Path.Combine(pending, "payload"), release.Tag, token), token).ConfigureAwait(false);
                token.ThrowIfCancellationRequested();
                Core.Write(Path.Combine(pending, "release.json"), Json.Encode(new { version = release.Tag }));
                File.Delete(archive);DownloadTransfer.Forget(Core.PathIn(root,".launcher/downloads/launcher"));
                return true;
            }
            catch
            {
                if(repair) { try { Core.DiscardRepairStaging(root,true); } catch { /* Retain protected or locked staging. */ } }
                else TryCleanup(root, ".launcher/self-pending", true);
                throw;
            }
        }
        public static void InstallPending(string root)
        {
            string pending = Core.PathIn(root, ".launcher/self-pending");
            string expected = Json.String(Json.Object(File.ReadAllText(Path.Combine(pending, "release.json"))), "version");
            var changes = ValidatePayload(Core.PathIn(pending, "payload"), expected);
            // A self update has its own recovery snapshot. BUILD.json and the
            // previous game-release snapshot are never part of this transaction.
            Core.ApplyLauncher(root, changes, null);
            Core.RemoveOwned(root, ".launcher/self-pending");
        }
        public static void WaitForParent(int pid)
        {
            try { using (var parent = Process.GetProcessById(pid)) Core.Require(parent.WaitForExit(30000), "The previous launcher did not close."); }
            catch (ArgumentException) { }
        }
        public static string Quote(string value)
        {
            Core.Require(!value.Contains("\""), "Invalid quoted path.");
            return "\"" + value + new string('\\', value.Length - value.TrimEnd('\\').Length) + "\"";
        }
        public static void StartApplication(string root, int parent)
        {
            string application = Core.PathIn(root, Core.AppPath);
            Core.Require(File.Exists(application), "Launcher application files are missing. Extract the complete launcher ZIP into this folder and try again.");
            Process.Start(new ProcessStartInfo(application, "--from-bootstrap " + parent) { WorkingDirectory = root, UseShellExecute = true });
        }
    }
}
