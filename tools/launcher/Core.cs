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
        public const string LauncherName = "TimeSplitters Launcher.exe";
        public const string AppName = "TimeSplitters.App.exe";
        public const string AppPath = "Bin/" + AppName;
        public const string SelfManifest = "Support/Launcher/manifest.json";
        public static bool LauncherFile(string name)
        {
            return name == LauncherName || name == AppPath || name == SelfManifest;
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
                    part.IndexOfAny(Path.GetInvalidFileNameChars()) < 0 && !Regex.IsMatch(part, @"^(CON|PRN|AUX|NUL|COM[0-9]|LPT[0-9])(?:\.|$)", RegexOptions.IgnoreCase),
                    "Unsafe Windows filename in update.");
            }
        }
        public static bool Allowed(string name)
        {
            Name(name);
            if (RootFiles.Contains(name)) return true;
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
        public static string PathIn(string root, string relative)
        {
            Name(relative);
            root = Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            string path = Path.GetFullPath(Path.Combine(root, relative.Replace('/', Path.DirectorySeparatorChar)));
            Require(path.StartsWith(root, StringComparison.OrdinalIgnoreCase), "Update path escaped its folder.");
            for (string cursor = path; cursor != null; cursor = Path.GetDirectoryName(cursor))
            {
                if (File.Exists(cursor) || Directory.Exists(cursor))
                    Require((File.GetAttributes(cursor) & FileAttributes.ReparsePoint) == 0, "Linked installation paths are not supported.");
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
            Require((File.GetAttributes(dir) & FileAttributes.ReparsePoint) == 0, "Refusing linked cleanup folder.");
            foreach (string file in Directory.GetFileSystemEntries(dir))
            {
                var attr = File.GetAttributes(file);
                Require((attr & FileAttributes.ReparsePoint) == 0, "Refusing linked cleanup path.");
                if ((attr & FileAttributes.Directory) != 0) CheckTree(file);
            }
        }
        public static string InstalledVersion(string root)
        {
            string path = PathIn(root, "BUILD.json");
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
        public static List<Change> Unpack(string archive, string destination, string expectedVersion)
        {
            Require(!Directory.Exists(destination), "An extraction folder already exists.");
            using (var zip = ZipFile.OpenRead(archive))
            {
                Require(zip.Entries.Count > 0 && zip.Entries.Count <= 20000, "Invalid release archive file count.");
                var manifests = zip.Entries.Where(e => e.FullName == "BUILD.json" || e.FullName.EndsWith("/BUILD.json", StringComparison.Ordinal)).ToArray();
                Require(manifests.Length == 1, "The release needs exactly one BUILD.json manifest.");
                var manifest = manifests[0];
                Require(manifest.Length <= 1024 * 1024, "Oversized build manifest.");
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
                Require(expected.ContainsKey("Bin/futureperfect_recomp.exe") && expected.ContainsKey("Launch.ps1") &&
                    expected.ContainsKey("Settings/pc-controls.json"), "The release is missing required runtime files.");
                long size = 0;
                var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
                var entries = new List<Tuple<ZipArchiveEntry, string>>();
                foreach (var entry in zip.Entries)
                {
                    // Even directory records must have safe names and belong to the one package root.
                    Name(entry.FullName.TrimEnd('/'));
                    Require(entry.FullName.StartsWith(prefix, StringComparison.Ordinal), "Mixed archive roots.");
                    Require(((entry.ExternalAttributes >> 16) & 0xF000) != 0xA000 && (entry.ExternalAttributes & 0x400) == 0, "Linked archive entries are not supported.");
                    if (entry.FullName.EndsWith("/", StringComparison.Ordinal)) continue;
                    string name = entry.FullName.Substring(prefix.Length);
                    Require(Allowed(name) && names.Add(name), "Duplicate or protected archive path: " + name);
                    Require(name == "BUILD.json" || expected.ContainsKey(name), "Unlisted archive file: " + name);
                    size += entry.Length;
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
                    string output = PathIn(destination, pair.Item2);
                    Directory.CreateDirectory(Path.GetDirectoryName(output));
                    using (var input = pair.Item1.Open()) using (var file = new FileStream(output, FileMode.CreateNew, FileAccess.Write)) input.CopyTo(file);
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
                    File.Copy(backup, target, true);
                }
                else if (File.Exists(target)) File.Delete(target);
            }
        }
        public static bool Recover(string root)
        { return RecoverScope(root, false); }
        public static bool RecoverLauncher(string root)
        { return RecoverScope(root, true); }
        static bool RecoverScope(string root, bool launcher)
        {
            string relative = launcher ? ".launcher/self-transaction" : ".launcher/transaction";
            string transaction = PathIn(root, relative);
            if (!Directory.Exists(transaction)) return false;
            if (File.Exists(Path.Combine(transaction, "journal.json"))) Restore(root, transaction, launcher);
            RemoveOwned(root, relative);
            return true;
        }
        public static void Apply(string root, List<Change> changes, Action<int> beforeChange)
        { ApplyScope(root, changes, beforeChange, false); }
        public static void ApplyLauncher(string root, List<Change> changes, Action<int> beforeChange)
        { ApplyScope(root, changes, beforeChange, true); }
        static void ApplyScope(string root, List<Change> changes, Action<int> beforeChange, bool launcher)
        {
            Require(!GameRunning(), "Close the game before updating or restoring a release.");
            RecoverScope(root, launcher);
            string relative = launcher ? ".launcher/self-transaction" : ".launcher/transaction";
            string rollback = launcher ? ".launcher/self-rollback" : ".launcher/rollback";
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
                    else { Directory.CreateDirectory(Path.GetDirectoryName(target)); File.Copy(change.Source, target, true); }
                }
                RemoveOwned(root, rollback);
                Directory.Move(transaction, PathIn(root, rollback));
            }
            catch
            {
                if (File.Exists(Path.Combine(transaction, "journal.json"))) Restore(root, transaction, launcher);
                RemoveOwned(root, relative);
                throw;
            }
        }
        public static bool CanRollback(string root)
        {
            string folder = PathIn(root, ".launcher/rollback");
            return File.Exists(Path.Combine(folder, "journal.json")) && File.Exists(Path.Combine(folder, "files", "BUILD.json"));
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
            string payload = PathIn(pending, "payload");
            // Re-validate files immediately before installation, including after a restart.
            var metadata = Json.Object(File.ReadAllText(PathIn(payload, "BUILD.json")));
            Require(Json.String(metadata, "version") == tag, "Staged version mismatch.");
            var hashes = (Dictionary<string, object>)metadata["files"];
            var changes = new List<Change>();
            string currentLauncher = SelfUpdate.InstalledVersion(root);
            string packagedLauncher = Json.String(metadata, "launcher_version");
            bool keepLauncher = currentLauncher.Length > 0 && (packagedLauncher.Length == 0 ||
                GameVersion.Parse(currentLauncher).CompareTo(GameVersion.Parse(packagedLauncher)) > 0);
            foreach (var pair in hashes)
            {
                Require(Allowed(pair.Key), "Protected staged file.");
                string path = PathIn(payload, pair.Key);
                Require(Hash(path).Equals(Convert.ToString(pair.Value), StringComparison.OrdinalIgnoreCase), "Staged file changed: " + pair.Key);
                if (!keepLauncher || !LauncherFile(pair.Key)) changes.Add(new Change { Name = pair.Key, Source = path });
            }
            Require(hashes.ContainsKey("Bin/futureperfect_recomp.exe") && hashes.ContainsKey("Launch.ps1"), "Staged runtime is incomplete.");
            changes.Add(new Change { Name = "BUILD.json", Source = PathIn(payload, "BUILD.json") });
            Apply(root, changes, null);
            RemoveOwned(root, ".launcher/pending");
        }
    }
}
