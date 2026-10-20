using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Security.Cryptography;
using System.Text.RegularExpressions;
using System.Threading;

namespace TimeSplitters.Launcher
{
    public sealed class Change { public string Name; public string Source; }
    public sealed class Original { public string Name; public bool Existed; }
    public static class Core
    {
        public const string LauncherName = LauncherPlatform.LauncherBinary;
        public const string AppName = "TimeSplitters.App.exe";
        public const string AppPath = LauncherPlatform.LauncherApplication;
        public const string SelfManifest = "Support/Launcher/manifest.json";
        public static bool LauncherFile(string name)
        {
            return name == LauncherName || name == AppPath || name == SelfManifest || LauncherPlatform.LauncherResource(name);
        }
        static readonly HashSet<string> RootFiles = new HashSet<string>(StringComparer.OrdinalIgnoreCase) {
            LauncherName, "Play.cmd", "Launch.ps1", "Start-game.ps1", "Import-game.ps1", "ImportGame.cs", "README.txt", "BUILD.json" };
        public static void Require(bool value, string message) { if (!value) throw new InvalidDataException(message); }
        public static string Hash(string path)
        {
            using (var input = File.OpenRead(path)) using (var sha = SHA256.Create())
                return BitConverter.ToString(sha.ComputeHash(input)).Replace("-", "").ToLowerInvariant();
        }
        public static void Name(string name)
        {
            Require(!String.IsNullOrEmpty(name) && !name.Contains('\\') && !Path.IsPathRooted(name), "Unsafe path in update.");
            foreach (string part in name.Split('/'))
            {
                Require(part.Length > 0 && part != "." && part != ".." && part.TrimEnd(' ', '.') == part &&
                    part.IndexOfAny(Path.GetInvalidFileNameChars()) < 0 && part.IndexOfAny(new[]{':','*','?','"','<','>','|'}) < 0 && !Regex.IsMatch(part, @"^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(?:\.|$)", RegexOptions.IgnoreCase),
                    "Unsafe Windows filename in update.");
            }
        }
        public static bool Allowed(string name)
        {
            Name(name);
            if (TextureFile(name)) return true;
            if (RootFiles.Contains(name) || name == LauncherPlatform.GameBinary || name == LauncherPlatform.LaunchScript || LauncherFile(name)) return true;
#if MACOS
            if (name.StartsWith("Bin/",StringComparison.Ordinal) && name.EndsWith(".dylib",StringComparison.Ordinal)) return true;
#endif
            var p = name.Split('/');
            // Existing public releases include their corresponding source archive
            // and a bundled Python/online-support tree. These are package-owned,
            // separate from the user's data, saves, settings and texture pack.
            if (p.Length >= 2 && (p[0] == "Source" || p[0] == "Support"))
                return !p.Any(part => part.Equals(".git", StringComparison.OrdinalIgnoreCase)) &&
                    !new[] { ".iso", ".xbe", ".pak", ".img" }.Contains(Path.GetExtension(name).ToLowerInvariant());
            if (p.Length != 2) return false;
            if (p[0].Equals("Bin", StringComparison.OrdinalIgnoreCase))
                return p[1].EndsWith(".exe", StringComparison.OrdinalIgnoreCase) || p[1].EndsWith(".dll", StringComparison.OrdinalIgnoreCase);
            if (p[0].Equals("Licenses", StringComparison.OrdinalIgnoreCase)) return p[1].EndsWith(".txt", StringComparison.OrdinalIgnoreCase);
            return name.Equals("Settings/pc-controls.json", StringComparison.OrdinalIgnoreCase);
        }
        public static void RequireUnlinked(string path, string message)
        {
            FileAttributes attributes;
            try { attributes = File.GetAttributes(path); }
            catch (FileNotFoundException) { return; }
            catch (DirectoryNotFoundException) { return; }
            Require((attributes & FileAttributes.ReparsePoint) == 0 ||
                LauncherPlatform.IsCloudPlaceholder(path), message);
        }
        internal static bool TextureFile(string name)
        { return Regex.IsMatch(name, @"\ATextures/[0-9a-f]{16}\.(dds|json)\z"); }
        public static string PathIn(string root, string relative)
        {
            Name(relative);
            root = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            string path = Path.GetFullPath(Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar)));
            Require(path.StartsWith(root, StringComparison.OrdinalIgnoreCase), "Update path escaped its folder.");
            for (string cursor = path; cursor != null; cursor = Path.GetDirectoryName(cursor))
            {
                RequireUnlinked(cursor, "Linked installation paths are not supported.");
            }
            return path;
        }
        public static void Write(string path, string text)
        {
            Directory.CreateDirectory(Path.GetDirectoryName(path));
            string tmp = path + ".new";
            File.WriteAllText(tmp, text, new System.Text.UTF8Encoding(false));
            if (File.Exists(path)) File.Replace(tmp, path, null);
            else File.Move(tmp, path);
        }
        public static void RemoveOwned(string root, string relative)
        {
            Require(relative.StartsWith(".launcher/", StringComparison.Ordinal) && relative.Split('/').Length == 2,
                "Cleanup is limited to launcher-owned folders.");
            string target = PathIn(root, relative);
            if (!Directory.Exists(target)) return;
            CheckTree(target);
            Directory.Delete(target, true);
        }
        static void CheckTree(string dir)
        {
            RequireUnlinked(dir, "Refusing linked cleanup folder.");
            foreach (string file in Directory.GetFileSystemEntries(dir))
            {
                var attr = File.GetAttributes(file);
                RequireUnlinked(file, "Refusing linked cleanup path.");
                if ((attr & FileAttributes.Directory) != 0) CheckTree(file);
            }
        }
        public static string InstalledVersion(string root)
        {
            string path = PathIn(root, LauncherPlatform.ManifestPath(root));
            return File.Exists(path) ? Json.String(Json.Object(File.ReadAllText(path)), "version") : "";
        }
        public static bool GameRunning()
        {
            foreach (var process in Process.GetProcessesByName("futureperfect_recomp")) { process.Dispose(); return true; }
            return false;
        }
        public static string MutexName(string root)
        {
            using (var sha = SHA256.Create())
                return "Local\\TSFP-Launcher-" + BitConverter.ToString(sha.ComputeHash(System.Text.Encoding.UTF8.GetBytes(Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar).ToUpperInvariant()))).Replace("-", "");
        }
        public static void CheckSpace(string root, long bytes)
        {
            Require(new DriveInfo(Path.GetPathRoot(Path.GetFullPath(root))).AvailableFreeSpace > bytes,
                "There is not enough free space for the download, extracted files and recovery copy.");
        }

        // This only extracts files named and hashed by BUILD.json. User directories
        // are rejected except the initial controls default, which is never replaced.
        public static List<Change> Unpack(string archive, string destination, string expectedVersion, CancellationToken token = default(CancellationToken))
        {
            token.ThrowIfCancellationRequested();
            Require(!Directory.Exists(destination), "An extraction folder already exists.");
            using (var zip = ZipFile.OpenRead(archive))
            {
                Require(zip.Entries.Count > 0 && zip.Entries.Count <= 60000, "Invalid release archive file count.");
                var manifests = zip.Entries.Where(e => e.FullName == "BUILD.json" || e.FullName.EndsWith("/BUILD.json", StringComparison.Ordinal)).ToArray();
                Require(manifests.Length == 1, "The release needs exactly one BUILD.json manifest.");
                var manifest = manifests[0];
                Require(manifest.Length <= 8 * 1024 * 1024, "Oversized build manifest.");
                string prefix = manifest.FullName.Substring(0, manifest.FullName.Length - "BUILD.json".Length);
                if (prefix.Length > 0) { Name(prefix.TrimEnd('/')); Require(prefix.Count(c => c == '/') == 1, "Invalid package root."); }
                string manifestText;
                using (var reader = new StreamReader(manifest.Open())) manifestText = reader.ReadToEnd();
                var metadata = Json.Object(manifestText);
                Require(Json.String(metadata, "version") == expectedVersion, "The downloaded package version does not match the release.");
                GameVersion.Parse(expectedVersion);
                object hashesObject;
                Require(metadata.TryGetValue("files", out hashesObject), "The package is missing file checksums.");
                var hashes = hashesObject as Dictionary<string, object>;
                Require(hashes != null && hashes.Count > 0, "Invalid file checksum manifest.");
                var expected = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                foreach (var pair in hashes)
                {
                    Require(Allowed(pair.Key) && pair.Key != "BUILD.json", "The manifest contains a protected or unsupported file: " + pair.Key);
                    string hash = Convert.ToString(pair.Value);
                    Require(Regex.IsMatch(hash, "^[0-9a-fA-F]{64}$") && !expected.ContainsKey(pair.Key), "Invalid or duplicate manifest hash.");
                    expected.Add(pair.Key, hash);
                }
                Require(expected.ContainsKey(LauncherPlatform.GameBinary) && expected.ContainsKey(LauncherPlatform.LaunchScript) &&
                    expected.ContainsKey("Settings/pc-controls.json"), "The release is missing required runtime files.");
                long size = 0, textureSize = 0;
                var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                var entries = new List<Tuple<ZipArchiveEntry, string>>();
                foreach (var entry in zip.Entries)
                {
                    token.ThrowIfCancellationRequested();
                    // Even directory records must have safe names and belong to the one package root.
                    Name(entry.FullName.TrimEnd('/'));
                    Require(entry.FullName.StartsWith(prefix, StringComparison.Ordinal), "Mixed archive roots.");
                    Require(((entry.ExternalAttributes >> 16) & 0xF000) != 0xA000 && (entry.ExternalAttributes & 0x400) == 0, "Linked archive entries are not supported.");
                    if (entry.FullName.EndsWith("/", StringComparison.Ordinal)) continue;
                    string name = entry.FullName.Substring(prefix.Length);
                    Require(Allowed(name) && names.Add(name), "Duplicate or protected archive path: " + name);
                    Require(name == "BUILD.json" || expected.ContainsKey(name), "Unlisted archive file: " + name);
                    size += entry.Length;
                    if (TextureFile(name)) {
                        textureSize += entry.Length;
                        Require(entry.Length > 0 && entry.Length <= (name.EndsWith(".json", StringComparison.Ordinal) ? 65536L : 24L*1024*1024) &&
                            textureSize <= 6L*1024*1024*1024, "The texture payload exceeds its budget.");
                    }
                    Require(entry.Length <= 2L * 1024 * 1024 * 1024 && size <= 8L * 1024 * 1024 * 1024, "The expanded release is too large.");
                    entries.Add(Tuple.Create(entry, name));
                }
                Require(expected.Keys.All(names.Contains), "The release is missing files listed in its manifest.");
                string spaceRoot = Directory.GetParent(destination).FullName;
                while (!Directory.Exists(spaceRoot)) spaceRoot = Directory.GetParent(spaceRoot).FullName;
                CheckSpace(spaceRoot, size * 2 + 128L * 1024 * 1024);
                Directory.CreateDirectory(destination);
                var changes = new List<Change>();
                foreach (var pair in entries)
                {
                    token.ThrowIfCancellationRequested();
                    string output = PathIn(destination, pair.Item2);
                    Directory.CreateDirectory(Path.GetDirectoryName(output));
                    using (var input = pair.Item1.Open()) using (var file = new FileStream(output, FileMode.CreateNew, FileAccess.Write)) {
                        var buffer = new byte[65536]; int read;
                        while ((read = input.Read(buffer,0,buffer.Length)) > 0) { token.ThrowIfCancellationRequested(); file.Write(buffer,0,read); }
                    }
                    if (pair.Item2 != "BUILD.json") Require(Hash(output).Equals(expected[pair.Item2], StringComparison.OrdinalIgnoreCase), "Package file checksum mismatch: " + pair.Item2);
                    changes.Add(new Change { Name = pair.Item2, Source = output });
                }
                return changes;
            }
        }
        static bool Mutable(string name) { return Allowed(name) || name == ".launcher/installed.json"; }
        static List<Original> ReadJournal(string folder)
        {
            return Json.Serializer().Deserialize<List<Original>>(File.ReadAllText(Path.Combine(folder, "journal.json")));
        }
        static void Restore(string root, string folder, bool launcher)
        {
            foreach (var item in ReadJournal(folder))
            {
                Require(Mutable(item.Name) && (!launcher || LauncherFile(item.Name)), "Protected path in recovery journal.");
                string target = PathIn(root, item.Name);
                if (item.Existed)
                {
                    string backup = PathIn(folder, "files/" + item.Name);
                    Require(File.Exists(backup), "Recovery file is missing. Keep the .launcher folder for repair.");
                    Directory.CreateDirectory(Path.GetDirectoryName(target));
                    LauncherPlatform.CopyFile(backup, target);
                }
                else if (File.Exists(target)) File.Delete(target);
            }
        }
        public static bool Recover(string root)
        { return RecoverScope(root, false) | RecoverScope(root, false, true); }
        public static bool RecoverLauncher(string root)
        { return RecoverScope(root, true); }
        static bool RecoverScope(string root, bool launcher, bool repair = false)
        {
            string relative = launcher ? ".launcher/self-transaction" : repair ? ".launcher/repair-transaction" : ".launcher/transaction";
            string transaction = PathIn(root, relative);
            if (!Directory.Exists(transaction)) return false;
            if (File.Exists(Path.Combine(transaction, "journal.json"))) Restore(root, transaction, launcher);
            if(repair)CheckGameCleanupTree(transaction,false);
            RemoveOwned(root, relative);
            return true;
        }
        public static void Apply(string root, List<Change> changes, Action<int> beforeChange)
        { ApplyScope(root, changes, beforeChange, false); }
        public static void ApplyLauncher(string root, List<Change> changes, Action<int> beforeChange)
        { ApplyScope(root, changes, beforeChange, true); }
        internal static void PromoteTransaction(string root, string relative, string rollback)
        {
            // A scanner or sync client can briefly hold a newly written directory
            // without delete sharing. Retry only the rename, never replay file writes.
            for (int attempt = 0; ; attempt++)
            {
                try { Directory.Move(PathIn(root, relative), PathIn(root, rollback)); return; }
                catch (Exception ex)
                {
                    int error = ex.HResult & 0xffff;
                    if (!(ex is IOException || ex is UnauthorizedAccessException) ||
                        (error != 5 && error != 32 && error != 33)) throw;
                    if (attempt == 20)
                        throw new IOException("Windows is blocking the update recovery folder. Close programs using this installation, then retry the update. Keep the .launcher folder for recovery.", ex);
                    Thread.Sleep(250);
                }
            }
        }
        static void ApplyScope(string root, List<Change> changes, Action<int> beforeChange, bool launcher, bool repair = false)
        {
            Require(!GameRunning(), "Close the game before updating or restoring a release.");
            RecoverScope(root, launcher, repair);
            string relative = launcher ? ".launcher/self-transaction" : repair ? ".launcher/repair-transaction" : ".launcher/transaction";
            string rollback = launcher ? ".launcher/self-rollback" : repair ? ".launcher/repair-rollback" : ".launcher/rollback";
            string transaction = PathIn(root, relative);
            Directory.CreateDirectory(transaction);
            var journal = new List<Original>();
            var unique = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
            try
            {
                // Complete all backups before writing any installed file.
                foreach (var change in changes)
                {
                    Require(Mutable(change.Name) && (!launcher || LauncherFile(change.Name)) && unique.Add(change.Name), "Invalid or duplicate installation target.");
                    string target = PathIn(root, change.Name);
                    if (change.Name.StartsWith("Settings/", StringComparison.OrdinalIgnoreCase) && File.Exists(target)) continue;
                    if (TextureFile(change.Name) && (File.Exists(Path.ChangeExtension(target, ".dds")) ||
                        File.Exists(Path.ChangeExtension(target, ".json")))) continue;
                    bool exists = File.Exists(target);
                    if (exists)
                    {
                        string backup = PathIn(transaction, "files/" + change.Name);
                        Directory.CreateDirectory(Path.GetDirectoryName(backup));
                        File.Copy(target, backup, false);
                    }
                    journal.Add(new Original { Name = change.Name, Existed = exists });
                }
                Write(Path.Combine(transaction, "journal.json"), Json.Encode(journal));
                int index = 0;
                foreach (var item in journal)
                {
                    if (beforeChange != null) beforeChange(index++);
                    var change = changes.First(c => c.Name == item.Name);
                    string target = PathIn(root, item.Name);
                    if (change.Source == null) { if (File.Exists(target)) File.Delete(target); }
                    else { Directory.CreateDirectory(Path.GetDirectoryName(target)); LauncherPlatform.CopyFile(change.Source, target); }
                }
                if (!launcher && Directory.Exists(PathIn(root, rollback))) CheckGameCleanupTree(PathIn(root, rollback), false);
                RemoveOwned(root, rollback);
                PromoteTransaction(root, relative, rollback);
            }
            catch (Exception updateError)
            {
                try
                {
                    if (File.Exists(Path.Combine(transaction, "journal.json"))) Restore(root, transaction, launcher);
                    if(repair)CheckGameCleanupTree(transaction,false);
                    RemoveOwned(root, relative);
                }
                catch (Exception recoveryError)
                {
                    throw new IOException("The update failed and recovery could not finish. Close programs using this installation and reopen the launcher. Keep the .launcher folder for recovery; see launcher.log for details.",
                        new AggregateException(updateError, recoveryError));
                }
                throw;
            }
        }
        public static bool CanRollback(string root)
        {
            string folder = PathIn(root, ".launcher/rollback");
            return File.Exists(Path.Combine(folder, "journal.json")) && File.Exists(Path.Combine(folder, "files", "BUILD.json"));
        }
        internal static Dictionary<string, string> GameFiles(string manifest)
        {
            Require(new FileInfo(manifest).Length <= 8 * 1024 * 1024, "Oversized installed game manifest.");
            var metadata = Json.Object(File.ReadAllText(manifest));
            GameVersion.Parse(Json.String(metadata, "version"));
            object value;
            Require(metadata.TryGetValue("files", out value) && value is Dictionary<string, object>, "Game manifest has no file checksums.");
            var result = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
            foreach (var pair in (Dictionary<string, object>)value)
            {
                string hash = Convert.ToString(pair.Value);
                Require(Allowed(pair.Key) && pair.Key != "BUILD.json" && !result.ContainsKey(pair.Key) &&
                    Regex.IsMatch(hash, "^[0-9a-fA-F]{64}$"), "Unsafe game manifest entry.");
                result.Add(pair.Key, hash);
            }
            Require(result.ContainsKey(LauncherPlatform.GameBinary) && result.ContainsKey(LauncherPlatform.LaunchScript), "Game manifest is incomplete.");
            return result;
        }
        internal static bool PreserveGameFile(string name)
        {
            // Legacy game manifests may use different filename casing. Keep
            // installed launchers protected on case-insensitive filesystems,
            // while LauncherFile still requires canonical package paths.
            return name.StartsWith("Settings/", StringComparison.OrdinalIgnoreCase) || TextureFile(name) ||
                new[] { LauncherName, AppPath, SelfManifest }.Contains(name, StringComparer.OrdinalIgnoreCase) ||
                LauncherPlatform.LauncherResource(name);
        }
        public static string GameBuildHash(string root)
        {
            try {
                string manifest = PathIn(root, "BUILD.json");
                Require(new FileInfo(manifest).Length <= 8 * 1024 * 1024, "Oversized game manifest.");
                return Hash(manifest);
            }
            catch (Exception ex) { LauncherLog.Write(root, "Game cleanup unavailable: " + ex.Message); return ""; }
        }
        static bool GameCleanupReady(string root, string expected)
        {
            // Only retired/cache files are deleted here, never the live runtime.
            // Installation and rollback retain their separate game-running guard.
            if (expected.Length != 64 || Directory.Exists(PathIn(root, ".launcher/transaction")) ||
                Directory.Exists(PathIn(root, ".launcher/self-transaction")) || Directory.Exists(PathIn(root, ".launcher/repair-transaction")) || GameBuildHash(root) != expected) return false;
            foreach (var pair in GameFiles(PathIn(root, "BUILD.json")))
            {
                // Controls are personal. Launcher files can be updated independently.
                if (PreserveGameFile(pair.Key)) continue;
                string path = PathIn(root, pair.Key);
                if (!File.Exists(path) || !Hash(path).Equals(pair.Value, StringComparison.OrdinalIgnoreCase)) return false;
            }
            return true;
        }
        public static void FinishGameSession(string root, string observedBuild, bool windowShown, int exitCode)
        {
            if (!windowShown || exitCode != 0) return;
            try
            {
                if (!GameCacheExists(root) || !GameCleanupReady(root, observedBuild)) return;
                Write(PathIn(root, ".launcher/game-confirmed.json"), Json.Encode(new { manifest_sha256 = observedBuild }));
                PruneGameFiles(root);
            }
            catch (Exception ex) { LauncherLog.Write(root, "Game cleanup deferred: " + ex.Message); }
        }
        public static void CleanupGameAfterStartup(string root)
        {
            try
            {
                if (!GameCacheExists(root)) return;
                string marker = PathIn(root, ".launcher/game-confirmed.json");
                if (!File.Exists(marker) || new FileInfo(marker).Length > 4096) return;
                string expected = Json.String(Json.Object(File.ReadAllText(marker)), "manifest_sha256");
                if (GameCleanupReady(root, expected)) PruneGameFiles(root);
            }
            catch (Exception ex) { LauncherLog.Write(root, "Game cleanup deferred: " + ex.Message); }
        }
        static bool GameCacheExists(string root)
        {
            return Directory.Exists(PathIn(root, ".launcher/rollback")) || Directory.Exists(PathIn(root, ".launcher/pending")) ||
                Directory.Exists(PathIn(root, ".launcher/game-obsolete")) || Directory.Exists(PathIn(root, ".launcher/repair-rollback"));
        }
        static void CheckGameCleanupTree(string folder, bool pending, string relative = "")
        {
            // ISO files are never cache, even if manually placed in an updater
            // folder. Refuse the entire cleanup before deleting any of its files.
            foreach (string entry in Directory.GetFileSystemEntries(folder))
            {
                var attributes = File.GetAttributes(entry);
                RequireUnlinked(entry, "Linked game cleanup path.");
                string name = Path.GetFileName(entry);
                if ((attributes & FileAttributes.Directory) != 0)
                {
                    // Only a verified flat keyed texture payload inside pending
                    // staging is disposable; installed textures stay protected.
                    if (pending && relative == "payload" && name == "Textures") {
                        long bytes = 0;
                        foreach (string texture in Directory.GetFileSystemEntries(entry)) {
                            var flags = File.GetAttributes(texture);
                            RequireUnlinked(texture, "Linked staged texture.");
                            Require((flags & FileAttributes.Directory) == 0 &&
                                TextureFile("Textures/" + Path.GetFileName(texture)), "Unexpected file in staged textures.");
                            long length = new FileInfo(texture).Length;
                            bytes += length;
                            Require(length <= (texture.EndsWith(".json", StringComparison.Ordinal) ? 65536L : 24L*1024*1024) &&
                                bytes <= 6L*1024*1024*1024, "Oversized staged texture payload.");
                        }
                        continue;
                    }
                    bool installationRoot = relative.Length == 0 || relative.Equals(pending ? "payload" : "files", StringComparison.OrdinalIgnoreCase);
                    Require(!installationRoot || (!new[] { "Saves", "Data", "Textures" }.Contains(name, StringComparer.OrdinalIgnoreCase) &&
                        (pending || !name.Equals("Settings", StringComparison.OrdinalIgnoreCase))), "Protected data inside game cleanup folder: " + entry);
                    CheckGameCleanupTree(entry, pending, relative.Length == 0 ? name : relative + "/" + name);
                }
                else Require(!Path.GetExtension(entry).Equals(".iso", StringComparison.OrdinalIgnoreCase), "ISO inside updater folder; files preserved: " + entry);
            }
        }
        static void RemoveGameCache(string root, string relative)
        {
            try
            {
                string folder = PathIn(root, relative);
                if (!Directory.Exists(folder)) return;
                CheckGameCleanupTree(folder, relative == ".launcher/pending");
                // Rename a finished rollback before deletion so a locked file
                // cannot leave a partially deleted snapshot offered for restore.
                if (relative == ".launcher/rollback")
                {
                    string retired = PathIn(root, ".launcher/game-obsolete");
                    if (Directory.Exists(retired)) return;
                    Directory.Move(folder, retired);
                    relative = ".launcher/game-obsolete";
                }
                RemoveOwned(root, relative);
            }
            catch (Exception ex) { LauncherLog.Write(root, "Game cleanup deferred for " + relative + ": " + ex.Message); }
        }
        public static void DiscardGameStaging(string root)
        {
            string pending = PathIn(root, ".launcher/pending");
            if (Directory.Exists(pending)) CheckGameCleanupTree(pending, true);
            RemoveOwned(root, ".launcher/pending");
        }
        public static void DiscardRepairStaging(string root,bool launcher)
        {
            if(!launcher){DiscardGameStaging(root);return;}
            string relative=".launcher/self-pending";
            string pending=PathIn(root,relative);
            if(Directory.Exists(pending))CheckGameCleanupTree(pending,true);
            RemoveOwned(root,relative);
        }
        static void PruneGameFiles(string root)
        {
            RemoveGameCache(root, ".launcher/game-obsolete");
            RemoveGameCache(root, ".launcher/repair-rollback");
            RemoveGameCache(root, ".launcher/rollback");
            string pending = PathIn(root, ".launcher/pending");
            if (!Directory.Exists(pending)) return;
            string marker = PathIn(pending, "release.json");
            bool obsolete = !File.Exists(marker);
            if (!obsolete)
            {
                try { obsolete = GameVersion.Parse(Json.String(Json.Object(File.ReadAllText(marker)), "version"))
                    .CompareTo(GameVersion.Parse(InstalledVersion(root))) <= 0; }
                catch (Exception ex) { LauncherLog.Write(root, "Game staging cleanup deferred: " + ex.Message); }
            }
            if (obsolete) RemoveGameCache(root, ".launcher/pending");
        }
        public static void Rollback(string root)
        {
            Require(CanRollback(root), "There is no previous installed release to restore.");
            string folder = PathIn(root, ".launcher/rollback");
            bool independentLauncher = File.Exists(PathIn(root, SelfManifest));
            var changes = ReadJournal(folder).Where(i => !i.Name.StartsWith("Settings/", StringComparison.OrdinalIgnoreCase) &&
                (!independentLauncher || !LauncherFile(i.Name)))
                .Select(i => new Change { Name = i.Name, Source = i.Existed ? PathIn(folder, "files/" + i.Name) : null }).ToList();
            Apply(root, changes, null);
        }
        public static void InstallPending(string root)
        {
            string pending = PathIn(root, ".launcher/pending");
            var release = Json.Object(File.ReadAllText(Path.Combine(pending, "release.json")));
            string tag = Json.String(release, "version");
            bool repair = Json.Flag(release, "repair");
            if (repair) Require(InstalledVersion(root) == tag, "The installed game version changed. Verify it again before repairing.");
            string payload = PathIn(pending, "payload");
            // Re-validate files immediately before installation, including after a restart.
            var metadata = Json.Object(File.ReadAllText(PathIn(payload, "BUILD.json")));
            Require(Json.String(metadata, "version") == tag, "Staged version mismatch.");
            var hashes = (Dictionary<string, object>)metadata["files"];
            var changes = new List<Change>();
            string currentLauncher = SelfUpdate.InstalledVersion(root);
            string packagedLauncher = Json.String(metadata, "launcher_version");
            int launcherOrder = currentLauncher.Length > 0 && packagedLauncher.Length > 0
                ? GameVersion.Parse(currentLauncher).CompareTo(GameVersion.Parse(packagedLauncher)) : 0;
            // Equal versions can have different embedded artwork. Keep a verified
            // independent launcher, while allowing a broken equal version to repair.
            bool keepLauncher = repair || Json.Flag(release,"game_only") || (currentLauncher.Length > 0 && (packagedLauncher.Length == 0 ||
                launcherOrder > 0 || (launcherOrder == 0 && SelfUpdate.Complete(root))));
            foreach (var pair in hashes)
            {
                Require(Allowed(pair.Key), "Protected staged file.");
                string path = PathIn(payload, pair.Key);
                Require(Hash(path).Equals(Convert.ToString(pair.Value), StringComparison.OrdinalIgnoreCase), "Staged file changed: " + pair.Key);
                if ((!keepLauncher || !LauncherFile(pair.Key)) &&
                    (!repair || (!PreserveGameFile(pair.Key) && (!File.Exists(PathIn(root,pair.Key)) ||
                        !Hash(PathIn(root,pair.Key)).Equals(Convert.ToString(pair.Value),StringComparison.OrdinalIgnoreCase)))))
                    changes.Add(new Change { Name = pair.Key, Source = path });
            }
            Require(hashes.ContainsKey(LauncherPlatform.GameBinary) && hashes.ContainsKey(LauncherPlatform.LaunchScript), "Staged runtime is incomplete.");
            // Remove only unchanged files explicitly owned by the old release.
            // These removals share the install journal and therefore roll back
            // normally if installation fails or the user restores the old game.
            string oldManifest = PathIn(root, "BUILD.json");
            if (!repair && File.Exists(oldManifest))
            {
                Dictionary<string, string> previous = null;
                try { previous = GameFiles(oldManifest); }
                catch (Exception ex) { LauncherLog.Write(root, "Old game file pruning skipped: " + ex.Message); }
                if (previous != null)
                {
                    var incoming = new HashSet<string>(hashes.Keys, StringComparer.OrdinalIgnoreCase);
                    foreach (var pair in previous)
                    {
                        if (incoming.Contains(pair.Key) || PreserveGameFile(pair.Key)) continue;
                        string path = PathIn(root, pair.Key);
                        if (File.Exists(path) && Hash(path).Equals(pair.Value, StringComparison.OrdinalIgnoreCase))
                            changes.Add(new Change { Name = pair.Key, Source = null });
                    }
                }
            }
            changes.Add(new Change { Name = "BUILD.json", Source = PathIn(payload, "BUILD.json") });
            ApplyScope(root, changes, null, false, repair);
            RemoveGameCache(root, ".launcher/pending");
        }
    }
}
