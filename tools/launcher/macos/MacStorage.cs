#if MACOS
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;

namespace TimeSplitters.Launcher
{
    // A Finder-installed bundle is an entry point. The writable installation,
    // including its updateable launcher, lives independently of that bundle.
    // Keeping a complete installation here reuses the existing transactional
    // updater, locks, recovery and runtime path contracts without symlinks.
    public static class MacStorage
    {
        public static string DefaultRoot => Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.UserProfile),
            "Library", "Application Support", "TimeSplitters");
        const string Preference = "storage-root.txt";
        public static bool Explicit(string[] args) => args.Contains("--root") || args.Contains("--portable");
        public static bool Portable(string[] args,string source) => Explicit(args) || File.Exists(Core.PathIn(source,".tsfp-portable"));
        public static string Resolve(string[] args, string source, string support = null)
        {
            if (Portable(args,source) || LauncherPlatform.Checkout(source)) return source;
            support = support ?? DefaultRoot;
            string preference = Core.PathIn(support, Preference);
            if (!File.Exists(preference)) return support;
            Core.Require(new FileInfo(preference).Length < 4096, "The saved game folder is invalid. Choose your existing folder again.");
            string selected = SaveLocations.Validate(File.ReadAllText(preference).TrimEnd('\r', '\n'));
            Core.Require(Directory.Exists(selected), "Your selected game folder is unavailable. Reconnect its drive or choose the folder again. Existing saves were kept.");
            return selected;
        }
        public static void Remember(string root, string support = null)
        {
            root = SaveLocations.Validate(root); support = support ?? DefaultRoot;
            Core.Write(Core.PathIn(support, Preference), root + "\n");
        }
        public static bool Same(string first, string second) => String.Equals(
            Path.GetFullPath(first).TrimEnd(Path.DirectorySeparatorChar),
            Path.GetFullPath(second).TrimEnd(Path.DirectorySeparatorChar), SaveLocations.Comparison);
        public static bool HasPersonalFiles(string root) => new[] { "Data", "Saves", "Settings", "Textures" }
            .Any(name => Directory.Exists(Core.PathIn(root, name)) && Directory.EnumerateFileSystemEntries(Core.PathIn(root, name)).Any());
        public static bool HasInstallation(string root) => HasPersonalFiles(root) || File.Exists(Core.PathIn(root,"BUILD.json"));
        public static string ExistingFolder(string folder)
        {
            folder = SaveLocations.Validate(folder);
            Core.Require(Directory.Exists(folder) && !LauncherPlatform.Checkout(folder), "Choose an existing installed game folder, not a source checkout.");
            Core.Require(HasPersonalFiles(folder) || File.Exists(Core.PathIn(folder, "BUILD.json")),
                "Choose the folder containing Data, Saves, Settings or Textures, rather than one of those subfolders.");
            return folder;
        }
        public static ProcessStartInfo StartInfo(string root, string[] args)
        {
            var info = new ProcessStartInfo("/usr/bin/open") { UseShellExecute=false,WorkingDirectory=root };
            foreach(string arg in new[]{"-n",Core.PathIn(root,"TimeSplitters Launcher.app"),"--args","--root",root})info.ArgumentList.Add(arg);
            // Forward --root, keeping helper and future launches independent of
            // the entry-point bundle. Ordinary opens still check for updates.
            foreach (string arg in args.Where(a => a == "--skip-self-update"))
                if (!info.ArgumentList.Contains(arg)) info.ArgumentList.Add(arg);
            return info;
        }
        public static void Prepare(string source, string destination)
        {
            source = SaveLocations.Validate(source); destination = SaveLocations.Validate(destination);
            for(var directory=new DirectoryInfo(destination);directory!=null;directory=directory.Parent)
                Core.Require(!directory.Name.EndsWith(".app",StringComparison.OrdinalIgnoreCase), "Choose a game folder outside application bundles.");
            Core.Require(!NativeInstallation.Translocated(destination), "Choose a permanent writable game folder.");
            Core.Require(!LauncherPlatform.Checkout(destination), "A source checkout cannot be used as an installed game folder.");
            Core.Require(!Core.GameRunning(), "Close the game before choosing a game folder.");
            Directory.CreateDirectory(Core.PathIn(destination, ".launcher"));
            using (LauncherPlatform.AcquireUpdateLock(destination)) {
                Core.RecoverLauncher(destination);
                // Never downgrade a managed launcher when Finder opens an older
                // downloaded bundle. A new downloaded bundle can upgrade it.
                string installed = SelfUpdate.InstalledVersion(destination);
                if (SelfUpdate.Complete(destination) && GameVersion.Parse(installed).CompareTo(GameVersion.Parse(LauncherVersion.Value)) >= 0) return;
                Core.Require(String.IsNullOrEmpty(installed) || GameVersion.Parse(installed).CompareTo(GameVersion.Parse(LauncherVersion.Value)) <= 0,
                    "Your newer launcher needs repair. Open it from the selected game folder and use Diagnostics to repair it, or download that version again.");
                Core.Require(!Directory.Exists(Core.PathIn(destination, ".launcher/self-pending")) &&
                    !Directory.Exists(Core.PathIn(destination, ".launcher/pending")) &&
                    !Directory.Exists(Core.PathIn(destination, ".launcher/transaction")),
                    "An update needs recovery. Open the launcher already in your game folder before replacing it.");
                var files = new Dictionary<string, string>(StringComparer.OrdinalIgnoreCase);
                string bundle = Core.PathIn(source, "TimeSplitters Launcher.app");
                Collect(source, bundle, files);
                Core.Require(files.Count <= LauncherPlatform.LauncherFileLimit, "The launcher bundle has too many files.");
                // Reconstruct the manifest from the complete running bundle so
                // moving just the .app to /Applications also works. The source
                // bundle is never modified (its code-signature seal is copied).
                string manifest = Json.Encode(new { schema = 1, version = LauncherVersion.Value, files = files });
                SelfUpdate.Manifest(manifest, LauncherVersion.Value);
                if(Same(source,destination)) {
                    Core.Write(Core.PathIn(destination,Core.SelfManifest),manifest);
                    return;
                }
                long size = files.Keys.Sum(name => new FileInfo(Core.PathIn(source, name)).Length);
                Core.CheckSpace(destination, checked(size * 2));
                const string staging = ".launcher/storage-launcher";
                Core.RemoveOwned(destination, staging);
                string stage = Core.PathIn(destination, staging); Directory.CreateDirectory(stage);
                try {
                    var changes = new List<Change>();
                    foreach (var pair in files) {
                        string target = Core.PathIn(stage, pair.Key); Directory.CreateDirectory(Path.GetDirectoryName(target));
                        LauncherPlatform.CopyFile(Core.PathIn(source, pair.Key), target);
                        Core.Require(Core.Hash(target) == pair.Value, "The launcher changed while being copied. Close it and retry.");
                        changes.Add(new Change { Name = pair.Key, Source = target });
                    }
                    string metadata = Core.PathIn(stage, Core.SelfManifest); Core.Write(metadata, manifest);
                    changes.Add(new Change { Name = Core.SelfManifest, Source = metadata });
                    Core.ApplyLauncher(destination, changes, null);
                    Core.Require(SelfUpdate.Complete(destination), "The copied launcher could not be verified.");
                } finally { Core.RemoveOwned(destination, staging); }
            }
        }
        static void Collect(string root, string directory, Dictionary<string, string> files)
        {
            // Do not recurse through links (including an ancestor of the app).
            string relative = Path.GetRelativePath(root, directory).Replace('\\', '/');
            Core.PathIn(root, relative);
            foreach (string entry in Directory.EnumerateFileSystemEntries(directory)) {
                relative = Path.GetRelativePath(root, entry).Replace('\\', '/'); Core.PathIn(root, relative);
                if (Directory.Exists(entry)) Collect(root, entry, files);
                else {
                    Core.Require(Core.LauncherFile(relative) && relative != Core.SelfManifest, "Unexpected launcher bundle file: " + relative);
                    files.Add(relative, Core.Hash(entry));
                }
            }
        }
    }
}
#endif
