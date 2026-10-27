using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Text.RegularExpressions;
using System.Threading;

namespace TimeSplitters.Launcher
{
    // Shared installer for WPF and Avalonia. Only the host file pickers differ.
    public static class TexturePack
    {
        public const long Budget = 6L * 1024 * 1024 * 1024;
        const int FileLimit = 24 * 1024 * 1024;
        const string Work = ".launcher/texture-install";
        static readonly Regex KeyFile = new Regex("^[0-9a-f]{16}\\.(dds|json)$");
        sealed class Item { public string Name; public long Size; public Func<Stream> Open; }
        public sealed class Journal
        {
            public string Owner = "tsfp-textures-v1";
            public bool Committed;
            public List<Original> Files = new List<Original>();
        }
        public static string Destination(string root) { return Core.PathIn(root, LauncherPlatform.Textures(root)); }
        public static string Summary(string root)
        {
            try {
                string dir = Destination(root);
                int count = Directory.Exists(dir) ? Directory.GetFiles(dir, "*.dds").Count(p => KeyFile.IsMatch(Path.GetFileName(p))) : 0;
                return count == 0 ? "No texture pack installed. Classic textures remain available." :
                    count.ToString("N0") + " replacement textures installed. Select Enhanced in the game's Display settings.";
            } catch (Exception e) { return "Texture folder needs attention: " + e.Message; }
        }
        static void SafeSource(string path)
        { Core.PathIn(Path.GetDirectoryName(Path.GetFullPath(path)), Path.GetFileName(path)); }
        static void CheckItem(string name, long size)
        {
            Core.Require(KeyFile.IsMatch(name), "Choose a texture pack containing keyed DDS files and optional matching JSON metadata.");
            Core.Require(size > 0 && size <= (name.EndsWith(".dds") ? FileLimit : 65536), "A texture pack file exceeds its supported size.");
        }
        static void Save(string root, Journal journal)
        { Core.Write(Core.PathIn(root, Work + "/journal.json"), Json.Encode(journal)); }
        static void Rollback(string root, Journal journal)
        {
            string dest = Destination(root);
            foreach (var row in journal.Files.AsEnumerable().Reverse()) {
                string target = Core.PathIn(dest, row.Name), backup = Core.PathIn(root, Work + "/old/" + row.Name);
                if (row.Existed) {
                    if (!File.Exists(backup)) continue; // Not moved yet, or already recovered.
                    if (File.Exists(target)) File.Delete(target);
                    File.Move(backup, target);
                } else if (File.Exists(target)) File.Delete(target);
            }
        }
        public static bool Recover(string root)
        {
            string work = Core.PathIn(root, Work);
            if (!Directory.Exists(work)) return false;
            Core.Require(!Core.GameRunning(), "Close the game before recovering an interrupted texture installation.");
            string path = Core.PathIn(root, Work + "/journal.json");
            Core.Require(File.Exists(path) && new FileInfo(path).Length <= 4 * 1024 * 1024,
                "Texture installation recovery metadata is missing or invalid. Files were kept for recovery.");
            var journal = Json.Serializer().Deserialize<Journal>(File.ReadAllText(path));
            Core.Require(journal != null && journal.Owner == "tsfp-textures-v1" && journal.Files != null &&
                journal.Files.Count <= 40000 && journal.Files.All(f => f != null && KeyFile.IsMatch(f.Name ?? "")) &&
                journal.Files.Select(f => f.Name).Distinct(StringComparer.OrdinalIgnoreCase).Count() == journal.Files.Count,
                "Texture installation recovery metadata is invalid. Files were kept.");
            if (!journal.Committed) Rollback(root, journal);
            Core.RemoveOwned(root, Work);
            return !journal.Committed;
        }
        // Match the legacy DDS acceptance rules in src/textures/texture_pack_format.h.
        internal static void ValidateDds(string path)
        {
            using (var stream = File.OpenRead(path)) using (var read = new BinaryReader(stream)) {
                Core.Require(stream.Length >= 128 && stream.Length <= FileLimit, "Invalid DDS size.");
                var words = new uint[32]; for (int i = 0; i < words.Length; i++) words[i] = read.ReadUInt32();
                Core.Require(words[0] == 0x20534444 && words[1] == 124 && words[19] == 32 && words[28] == 0 && words[6] == 0, "Unsupported DDS header.");
                uint w = words[4], h = words[3], levels = 1;
                Core.Require(w > 0 && h > 0 && w <= 4096 && h <= 4096 && (w & (w - 1)) == 0 && (h & (h - 1)) == 0, "Textures must use power-of-two dimensions up to 4K.");
                for (uint n = Math.Max(w, h); n > 1; n >>= 1) levels++;
                Core.Require(words[7] == levels, "Texture is missing its complete mip chain.");
                uint block = 0;
                if (words[20] == 4) {
                    Core.Require(words[21] == 0x31545844 || words[21] == 0x33545844 || words[21] == 0x35545844, "Unsupported DDS compression.");
                    block = words[21] == 0x31545844 ? 8u : 16u;
                } else Core.Require(words[20] == 0x41 && words[21] == 0 && words[22] == 32 &&
                    words[23] == 0xff0000 && words[24] == 0xff00 && words[25] == 0xff && words[26] == 0xff000000, "Unsupported DDS pixel format.");
                long expected = 128;
                for (int i = 0; i < levels; i++) {
                    expected += block == 0 ? w * h * 4 : ((w + 3) / 4) * ((h + 3) / 4) * block;
                    w = Math.Max(1u, w / 2); h = Math.Max(1u, h / 2);
                }
                Core.Require(expected == stream.Length, "DDS mip data is truncated or has unexpected trailing bytes.");
            }
        }
        static void Copy(Item item, string target, CancellationToken token)
        {
            using (var input = item.Open()) using (var output = new FileStream(target, FileMode.CreateNew, FileAccess.Write, FileShare.None)) {
                var buffer = new byte[65536]; long copied = 0; int count;
                while ((count = input.Read(buffer, 0, buffer.Length)) != 0) {
                    token.ThrowIfCancellationRequested(); copied += count;
                    Core.Require(copied <= item.Size, "Texture source changed or exceeds its declared size.");
                    output.Write(buffer, 0, count);
                }
                Core.Require(copied == item.Size, "Texture source is incomplete or changed during installation.");
                output.Flush(true);
            }
        }
        static long InstalledBytes(string directory)
        {
            if (!Directory.Exists(directory)) return 0;
            long bytes = 0;
            // The loader uses a flat pack. Do not silently remove or ignore unknown trees.
            foreach (string path in Directory.EnumerateFileSystemEntries(directory)) {
                Core.RequireUnlinked(path, "Linked texture files are not supported.");
                Core.Require(!Directory.Exists(path), "The texture folder contains a nested folder. Its contents were kept; inspect it before installing.");
                bytes = checked(bytes + new FileInfo(path).Length);
            }
            return bytes;
        }
        public static int Install(string root, string source, IProgress<string> progress, CancellationToken token)
        {
            Core.Require(!Core.GameRunning(), "Close the game before installing textures.");
            Recover(root);
            source = Path.GetFullPath(source); SafeSource(source);
            string destination = Destination(root);
            string sourcePrefix = source.TrimEnd(Path.DirectorySeparatorChar) + Path.DirectorySeparatorChar;
            Core.Require(!destination.Equals(source, StringComparison.OrdinalIgnoreCase) &&
                !destination.StartsWith(sourcePrefix, StringComparison.OrdinalIgnoreCase) &&
                !source.StartsWith(destination + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase),
                "This pack is already in the game's texture folder. Choose a separate source pack.");
            if (Directory.Exists(source)) {
                var items = new List<Item>();
                foreach (string path in Directory.EnumerateFileSystemEntries(source)) {
                    SafeSource(path); Core.Require(!Directory.Exists(path), "Choose the folder containing the DDS files directly.");
                    string file = path;
                    items.Add(new Item { Name = Path.GetFileName(file), Size = new FileInfo(file).Length, Open = () => { SafeSource(file); return File.OpenRead(file); } });
                    Core.Require(items.Count <= 40000, "Texture pack has too many files.");
                }
                return InstallItems(root, items, progress, token);
            }
            Core.Require(Path.GetExtension(source).Equals(".zip", StringComparison.OrdinalIgnoreCase), "Choose a texture pack ZIP or folder.");
            using (var zip = ZipFile.OpenRead(source)) {
                Core.Require(zip.Entries.Count <= 40001, "Texture archive has too many entries.");
                var items = new List<Item>(); string prefix = null;
                foreach (var entry in zip.Entries) {
                    Core.Name(entry.FullName.TrimEnd('/'));
                    int kind = (entry.ExternalAttributes >> 16) & 0xf000;
                    Core.Require(kind == 0 || kind == 0x8000 || kind == 0x4000, "Linked or special archive entries are not supported.");
                    if (entry.FullName.EndsWith("/")) {
                        Core.Require(entry.FullName.Count(c => c == '/') == 1 && entry.Length == 0, "Texture archives must contain one flat pack folder.");
                        continue;
                    }
                    var pieces = entry.FullName.Split('/');
                    Core.Require(pieces.Length <= 2 && kind != 0x4000, "Texture archives must contain one flat pack folder.");
                    string parent = pieces.Length == 1 ? "" : pieces[0];
                    if (prefix == null) prefix = parent;
                    Core.Require(parent == prefix, "Texture archive contains multiple pack folders.");
                    var file = entry;
                    items.Add(new Item { Name = pieces.Last(), Size = file.Length, Open = () => file.Open() });
                }
                return InstallItems(root, items, progress, token);
            }
        }
        static int InstallItems(string root, List<Item> items, IProgress<string> progress, CancellationToken token)
        {
            token.ThrowIfCancellationRequested();
            Core.Require(items.Count > 0 && items.Select(f => f.Name).Distinct(StringComparer.OrdinalIgnoreCase).Count() == items.Count, "Texture pack is empty or contains duplicate names.");
            foreach (var item in items) CheckItem(item.Name, item.Size);
            var dds = items.Where(f => f.Name.EndsWith(".dds")).ToList();
            Core.Require(dds.Count > 0, "No replacement textures found.");
            foreach (var item in items.Where(f => f.Name.EndsWith(".json")))
                Core.Require(dds.Any(f => f.Name == Path.ChangeExtension(item.Name, ".dds")), "Texture metadata has no matching DDS file.");
            string destination = Destination(root);
            long total = InstalledBytes(destination), incoming = items.Sum(f => f.Size);
            foreach (var item in items) {
                string old = Core.PathIn(destination, item.Name);
                total += item.Size - (File.Exists(old) ? new FileInfo(old).Length : 0);
            }
            Core.Require(total <= Budget && incoming <= Budget, "This installation would exceed the 6 GiB texture-pack limit.");
            Core.CheckSpace(root, incoming + 64L * 1024 * 1024);
            string work = Core.PathIn(root, Work); Directory.CreateDirectory(work);
            var journal = new Journal(); Save(root, journal);
            Directory.CreateDirectory(Core.PathIn(root, Work + "/old"));
            Directory.CreateDirectory(destination);
            try {
                int index = 0;
                foreach (var image in dds) {
                    token.ThrowIfCancellationRequested();
                    if (progress != null) progress.Report("Installing texture " + (++index) + " of " + dds.Count + "…");
                    string staged = Core.PathIn(root, Work + "/texture.tmp");
                    Copy(image, staged, token); ValidateDds(staged);
                    var meta = items.FirstOrDefault(f => f.Name == Path.ChangeExtension(image.Name, ".json"));
                    string metadata = Core.PathIn(root, Work + "/metadata.tmp");
                    if (meta != null) {
                        Copy(meta, metadata, token);
                        var value = Json.Object(File.ReadAllText(metadata));
                        Core.Require(Json.String(value, "key") == Path.GetFileNameWithoutExtension(image.Name) &&
                            Json.String(value, "sha256").Equals(Core.Hash(staged), StringComparison.OrdinalIgnoreCase),
                            "Texture metadata checksum or key does not match " + image.Name + ".");
                    } else {
                        // Replace stale metadata when updating a DDS-only pack.
                        File.WriteAllText(metadata, Json.Encode(new { key = Path.GetFileNameWithoutExtension(image.Name), sha256 = Core.Hash(staged) }));
                        total += new FileInfo(metadata).Length - (File.Exists(Core.PathIn(destination, Path.ChangeExtension(image.Name, ".json"))) ?
                            new FileInfo(Core.PathIn(destination, Path.ChangeExtension(image.Name, ".json"))).Length : 0);
                        Core.Require(total <= Budget, "Texture metadata would exceed the 6 GiB texture-pack limit.");
                    }
                    token.ThrowIfCancellationRequested();
                    string existing=Core.PathIn(destination,image.Name), existingMeta=Core.PathIn(destination,Path.ChangeExtension(image.Name,".json"));
                    if(File.Exists(existing) && File.Exists(existingMeta) && Core.Hash(existing)==Core.Hash(staged) && Core.Hash(existingMeta)==Core.Hash(metadata)) {
                        File.Delete(staged);File.Delete(metadata);continue;
                    }
                    Apply(root, journal, image.Name, staged);
                    Apply(root, journal, Path.ChangeExtension(image.Name, ".json"), metadata);
                }
                token.ThrowIfCancellationRequested();
                journal.Committed = true; Save(root, journal);
            } catch {
                Rollback(root, journal);
                Core.RemoveOwned(root, Work);
                throw;
            }
            // A cleanup failure is recoverable on the next start; the pack is committed.
            try { Core.RemoveOwned(root, Work); } catch (IOException) { } catch (UnauthorizedAccessException) { }
            return dds.Count;
        }
        static void Apply(string root, Journal journal, string name, string staged)
        {
            string target = Core.PathIn(Destination(root), name), backup = Core.PathIn(root, Work + "/old/" + name);
            var row = new Original { Name = name, Existed = File.Exists(target) };
            journal.Files.Add(row); Save(root, journal); // Durable intent before either rename.
            if (row.Existed) File.Move(target, backup);
            File.Move(staged, target);
        }
    }
}
