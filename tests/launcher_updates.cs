// Uses tiny synthetic packages only. Never launches the game or touches real data.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using TimeSplitters.Launcher;

static class LauncherTests
{
    static int passed;
    static string temp;
    static void Test(string name, Action body) { body(); passed++; Console.WriteLine("PASS " + name); }
    static void Assert(bool value) { if (!value) throw new Exception("Assertion failed."); }
    static void Reject(Action action)
    {
        try { action(); } catch (InvalidDataException) { return; } catch (IOException) { return; }
        throw new Exception("Expected rejection.");
    }
    static string Hash(string text)
    { return HashBytes(Encoding.UTF8.GetBytes(text)); }
    static string HashBytes(byte[] bytes)
    {
        using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "").ToLowerInvariant();
    }
    static void Put(string root, string name, string text)
    {
        string path = Path.Combine(root, name.Replace('/', Path.DirectorySeparatorChar));
        Directory.CreateDirectory(Path.GetDirectoryName(path)); File.WriteAllText(path, text, new UTF8Encoding(false));
    }
    static Dictionary<string, string> Files()
    {
        return new Dictionary<string, string> { { "Bin/futureperfect_recomp.exe", "new game" }, { "Launch.ps1", "launch" },
            { "Settings/pc-controls.json", "defaults" }, { Core.LauncherName, "new launcher" }, { "Licenses/Runtime.txt", "license" } };
    }
    static string Zip(string caseName, Action<Dictionary<string, string>> alter, string extra, bool wrongHash)
    {
        var files = Files(); if (alter != null) alter(files);
        var hashes = files.ToDictionary(p => p.Key, p => Hash(p.Value));
        if (wrongHash) hashes["Launch.ps1"] = new string('0', 64);
        string path = Path.Combine(temp, caseName + ".zip");
        using (var zip = ZipFile.Open(path, ZipArchiveMode.Create))
        {
            foreach (var file in files)
                using (var writer = new StreamWriter(zip.CreateEntry("package/" + file.Key).Open(), new UTF8Encoding(false))) writer.Write(file.Value);
            using (var writer = new StreamWriter(zip.CreateEntry("package/BUILD.json").Open(), new UTF8Encoding(false)))
                writer.Write(Json.Encode(new { version = "v1.1.0", files = hashes }));
            if (extra != null) using (var writer = new StreamWriter(zip.CreateEntry(extra).Open())) writer.Write("unlisted");
        }
        return path;
    }
    static object Asset(string tag)
    {
        string name = "TimeSplitters-Future-Perfect-PC-" + tag + "-windows-x64.zip";
        return new { name = name, state = "uploaded", size = 100, digest = "sha256:" + new string('a', 64),
            browser_download_url = "https://github.com/" + ReleaseClient.Repository + "/releases/download/" + tag + "/" + name };
    }
    static object Release(string tag, bool pre, bool draft)
    { return new { tag_name = tag, prerelease = pre, draft = draft, body = "Notes", assets = new object[] { Asset(tag) } }; }
    static string NewRoot(string name)
    {
        string root = Path.Combine(temp, name); Directory.CreateDirectory(root);
        Put(root, "BUILD.json", "{\"version\":\"v1.0.0\"}");
        Put(root, "Bin/futureperfect_recomp.exe", "old game");
        Put(root, Core.LauncherName, "old launcher");
        Put(root, "Settings/pc-controls.json", "personal bindings");
        Put(root, "Settings/pc-display.ini", "personal display");
        Put(root, "Settings/runtime.conf", "personal runtime");
        Put(root, "Saves/profile", "personal save"); Put(root, "Data/default.xbe", "personal data");
        Put(root, "Textures/pack.dds", "personal textures");
        return root;
    }
    static void Personal(string root)
    {
        Assert(File.ReadAllText(Path.Combine(root, "Settings/pc-controls.json")) == "personal bindings");
        Assert(File.ReadAllText(Path.Combine(root, "Settings/pc-display.ini")) == "personal display");
        Assert(File.ReadAllText(Path.Combine(root, "Settings/runtime.conf")) == "personal runtime");
        Assert(File.ReadAllText(Path.Combine(root, "Saves/profile")) == "personal save");
        Assert(File.ReadAllText(Path.Combine(root, "Data/default.xbe")) == "personal data");
        Assert(File.ReadAllText(Path.Combine(root, "Textures/pack.dds")) == "personal textures");
    }
    sealed class Handler : HttpMessageHandler
    {
        public Func<HttpRequestMessage, HttpResponseMessage> Reply;
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken token)
        { var response = Reply(request); response.RequestMessage = request; return Task.FromResult(response); }
    }
    sealed class Progress : IProgress<double> { public void Report(double value) { } }
    sealed class SlowHandler : HttpMessageHandler
    {
        protected override async Task<HttpResponseMessage> SendAsync(HttpRequestMessage request, CancellationToken token)
        {
            // Deliberately ignore cancellation to model a stalled OS network call.
            await Task.Delay(500);
            return new HttpResponseMessage(HttpStatusCode.OK) { RequestMessage = request, Content = new StringContent("[]") };
        }
    }
    static string SelfManifest(string version, string boot, string app)
    {
        return Json.Encode(new { schema = 1, version = version, files = new Dictionary<string, string> { { Core.LauncherName, Hash(boot) }, { Core.AppPath, Hash(app) } } });
    }
    static void PutSelf(string root, string version)
    {
        Put(root, Core.LauncherName, "bootstrap " + version); Put(root, Core.AppPath, "app " + version);
        Put(root, Core.SelfManifest, SelfManifest(version, "bootstrap " + version, "app " + version));
    }
    static string SelfZip(string name, string version, string extra, bool corrupt)
    {
        string path = Path.Combine(temp, name + ".zip");
        using (var zip = ZipFile.Open(path, ZipArchiveMode.Create))
        {
            var files = new Dictionary<string, string> { { Core.LauncherName, "bootstrap " + version }, { Core.AppPath, corrupt ? "corrupt" : "app " + version },
                { Core.SelfManifest, SelfManifest(version, "bootstrap " + version, "app " + version) } };
            if (extra != null) files.Add(extra, "unexpected");
            foreach (var file in files)
                using (var writer = new StreamWriter(zip.CreateEntry(file.Key).Open(), new UTF8Encoding(false))) writer.Write(file.Value);
        }
        return path;
    }
    static object SelfRelease(string version, bool preview, bool draft, long size, string digest)
    {
        string name = "TimeSplitters-Launcher-v" + version + "-windows-x64.zip";
        string prefix = "https://github.com/" + ReleaseClient.Repository + "/releases/download/launcher-v" + version + "/";
        return new { tag_name = "launcher-v" + version, prerelease = preview, draft = draft, assets = new object[] {
            new { name = name, state = "uploaded", size = size, digest = digest, browser_download_url = prefix + name },
            new { name = "launcher-update.json", state = "uploaded", size = 500, digest = "", browser_download_url = prefix + "launcher-update.json" } } };
    }
    static string SelfMetadata(string version, long size, string hash)
    { return Json.Encode(new { schema = 1, version = version, size = size, sha256 = hash, asset_name = "TimeSplitters-Launcher-v" + version + "-windows-x64.zip" }); }
    public static int Main(string[] args)
    {
        if (args.Length == 2 && (args[0] == "--render-preview" || args[0] == "--render-settings-preview" || args[0] == "--render-settings-graphics" || args[0] == "--render-repair-preview" || args[0] == "--render-diagnostics-preview" || args[0] == "--render-news-preview" || args[0] == "--render-versions-preview"))
        { File.WriteAllText(args[1],args[0]);return 17; } // Synthetic preview child, not a UI/game launch.
        if (args.Length == 1 && args[0] == "--settings-child") return LauncherSettingsChecks.Child();
        if (args.Length == 1 && args[0] == "--maintenance-readonly") return LauncherMaintenanceChecks.Run(true);
        if (args.Length == 1 && args[0] == "--maintenance") return LauncherMaintenanceChecks.Run();
        if (args.Length == 1 && args[0] == "--downloads-live") return LauncherDownloadChecks.VerifyPublicDownload();
        if (args.Length == 1 && args[0] == "--downloads") return LauncherDownloadChecks.Run();
        if (args.Length == 1 && args[0] == "--installer-registration") return LauncherInstallerChecks.Run();
        if (args.Length == 1 && args[0] == "--versions") return LauncherVersionChecks.Run();
        if (args.Length == 1 && args[0] == "--news") return LauncherNewsChecks.Run();
        if (args.Length == 1 && args[0] == "--settings") return LauncherSettingsChecks.Run();
        if (args.Length == 2 && args[0] == "--helper")
            return Program.RunHelper("--apply", args[1], Int32.MaxValue, false);
        if (args.Length == 2 && args[0] == "--self-helper")
            return Bootstrap.RunHelper(args[1], Int32.MaxValue, false);
        if (args.Length == 1 && args[0] == "--live-launcher")
        {
            using (var client = new ReleaseClient())
                foreach (bool preview in new[] { false, true })
                {
                    var release = SelfUpdate.CheckWithTimeout(client, preview, TimeSpan.FromSeconds(8), CancellationToken.None).GetAwaiter().GetResult();
                    Console.WriteLine((preview ? "Preview" : "Stable") + " launcher: " + (release == null ? "No standalone launcher release published" : release.Tag));
                }
            return 0;
        }
        if (args.Length == 1 && args[0] == "--verify-release")
            return VerifyPublishedRelease();
        if (args.Length == 1 && args[0] == "--live")
        {
            using (var client = new ReleaseClient())
            using (var timeout = new CancellationTokenSource(30000))
                foreach (bool preview in new[] { false, true })
                {
                    var release = client.Check(preview, timeout.Token).GetAwaiter().GetResult();
                    Console.WriteLine((preview ? "Preview" : "Stable") + ": " + (release == null ? "No compatible release" : release.Tag + " | " + release.AssetName + " | " + release.Size));
                }
            return 0;
        }
        temp = Path.Combine(Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location), "updater-tests");
        if (Directory.Exists(temp)) throw new Exception("Remove the previous isolated updater-tests folder before repeating.");
        Directory.CreateDirectory(temp);
        try
        {
            if (args.Length == 1 && args[0] == "--cleanup") {
                CleanupTests(); Console.WriteLine("PASS: " + passed + " isolated launcher cleanup checks."); return 0;
            }
            if (args.Length == 1 && args[0] == "--game-cleanup") {
                GameCleanupTests(); Console.WriteLine("PASS: " + passed + " isolated game cleanup checks."); return 0;
            }
            Test("bootstrap forwards application previews and preserves spaced output paths and child status", () => {
                string root=Path.Combine(temp,"preview launcher root");Directory.CreateDirectory(Path.Combine(root,"Bin"));
                File.Copy(typeof(LauncherTests).Assembly.Location,Path.Combine(root,Core.AppPath));
                foreach(string flag in new[]{"--render-preview","--render-settings-preview","--render-settings-graphics","--render-repair-preview","--render-diagnostics-preview","--render-news-preview","--render-versions-preview"}) {
                    string output=Path.Combine(root,flag.Substring(2)+" image.png");int result;
                    Assert(Bootstrap.TryForwardPreview(root,new[]{flag,output},out result));
                    Assert(result==17 && File.ReadAllText(output)==flag);
                }
                string previous=Environment.CurrentDirectory;
                try {
                    Environment.CurrentDirectory=temp;int result;
                    Assert(Bootstrap.TryForwardPreview(root,new[]{"--render-preview","relative preview.png"},out result));
                    Assert(result==17 && File.ReadAllText(Path.Combine(temp,"relative preview.png"))=="--render-preview");
                } finally { Environment.CurrentDirectory=previous; }
                Assert(!Directory.Exists(Path.Combine(root,"Settings")) && !Directory.Exists(Path.Combine(root,"Data")));
            });
            Test("bootstrap preserves normal startup, splash and updater argument routes", () => {
                foreach(var input in new[]{new string[0],new[]{"--after-self-update"},new[]{"--render-splash","output.png"},new[]{"--apply-launcher","root","123"},new[]{"--from-bootstrap","123"},new[]{"--unknown","value"},new[]{"--render-preview"}}) {
                    int result;Assert(!Bootstrap.TryForwardPreview(temp,input,out result));
                }
            });
            Test("rejected bootstrap argument diagnostics are escaped and bounded", () => {
                Assert(Bootstrap.DescribeArguments(new string[0])=="[]");
                string text=Bootstrap.DescribeArguments(new[]{"--bad\nargument",new string('x',10000)});
                Assert(!text.Contains("\n") && text.Contains("\\n") && text.Length<400);
                Assert(Bootstrap.DescribeArguments(Enumerable.Repeat("argument",20).ToArray()).Contains("additional arguments omitted"));
            });
            if (args.Length == 1 && args[0] == "--bootstrap") {
                Console.WriteLine("PASS: " + passed + " isolated bootstrap checks.");return 0;
            }
            CleanupTests();
            GameCleanupTests();
            Test("semantic version ordering", () => {
                var ordered = new[] { "v1.0.0-alpha", "v1.0.0-alpha.2", "v1.0.0-alpha.10", "v1.0.0-beta", "v1.0.0", "v1.0.1", "v1.10.0", "v2.0.0" };
                for (int i = 1; i < ordered.Length; i++) Assert(GameVersion.Parse(ordered[i]).CompareTo(GameVersion.Parse(ordered[i-1])) > 0);
                Reject(() => GameVersion.Parse("v1.0.0-alpha.01")); Reject(() => GameVersion.Parse("../main"));
            });
            Test("stable excludes drafts and previews", () => {
                var json = Json.Encode(new[] { Release("v5.0.0", false, true), Release("v3.0.0-beta.1", true, false), Release("v1.0.0", false, false), Release("v2.0.0", false, false) });
                Assert(ReleaseClient.Select(json, false).Tag == "v2.0.0");
                Assert(ReleaseClient.Select(json, true).Tag == "v3.0.0-beta.1");
            });
            Test("stable rejects prerelease tag even if mislabeled", () => Assert(ReleaseClient.Select(Json.Encode(Release("v1.0.0-alpha", false, false)), false) == null));
            Test("stable latest endpoint object", () => Assert(ReleaseClient.Select(Json.Encode(Release("v1.0.0", false, false)), false).Tag == "v1.0.0"));
            Test("source-only releases ignored", () => Assert(ReleaseClient.Select("{\"tag_name\":\"v1.0.0\",\"assets\":[]}", false) == null));
            Test("unverified archives ignored", () => {
                var obj = Json.Object(Json.Encode(Release("v1.0.0", false, false)));
                ((Dictionary<string, object>)((System.Collections.IList)obj["assets"])[0])["digest"] = null;
                Assert(ReleaseClient.Select(Json.Encode(obj), false) == null);
            });
            Test("download URLs pinned to project", () => {
                foreach (string url in new[] { "http://github.com/" + ReleaseClient.Repository + "/releases/download/a/b", "https://evil.example/a.zip", "https://github.com/other/repo/releases/download/a/b", "https://github.com:8080/" + ReleaseClient.Repository + "/releases/download/a/b" }) Reject(() => ReleaseClient.ValidateDownloadUrl(url));
            });
            foreach (string name in new[] { "../outside.exe", "/absolute.exe", "C:/file.exe", "Bin/../file.exe", "Bin/x:stream", "Bin/CON.dll", "Bin/file. ", "Bin\\file.dll", "Bin//file.dll" })
            { string value = name; Test("reject path " + value, () => Reject(() => Core.Allowed(value))); }
            foreach (string name in new[] { "Data/default.xbe", "Saves/profile", "Settings/pc-display.ini", "Textures/a.dll", "private/file", ".launcher/update-helper.exe" })
            { string value = name; Test("protect " + value, () => Assert(!Core.Allowed(value))); }
            Test("valid archive verifies and extracts", () => {
                var changes = Core.Unpack(Zip("valid", null, null, false), Path.Combine(temp, "valid-payload"), "v1.1.0");
                Assert(changes.Count == 6);
            });
            Test("wrong archive version rejected", () => Reject(() => Core.Unpack(Path.Combine(temp, "valid.zip"), Path.Combine(temp, "wrong-version"), "v2.0.0")));
            Test("wrong file checksum rejected", () => Reject(() => Core.Unpack(Zip("bad-hash", null, null, true), Path.Combine(temp, "bad-hash-payload"), "v1.1.0")));
            Test("zip traversal rejected", () => Reject(() => Core.Unpack(Zip("traversal", null, "package/../outside.exe", false), Path.Combine(temp, "traversal-payload"), "v1.1.0")));
            Test("unlisted executable rejected", () => Reject(() => Core.Unpack(Zip("extra", null, "package/Bin/extra.exe", false), Path.Combine(temp, "extra-payload"), "v1.1.0")));
            Test("case duplicate rejected", () => Reject(() => Core.Unpack(Zip("duplicate", null, "package/bin/FUTUREPERFECT_RECOMP.exe", false), Path.Combine(temp, "duplicate-payload"), "v1.1.0")));
            Test("protected data in manifest rejected", () => Reject(() => Core.Unpack(Zip("protected", f => f.Add("Data/default.xbe", "bad"), null, false), Path.Combine(temp, "protected-payload"), "v1.1.0")));
            Test("missing runtime rejected", () => Reject(() => Core.Unpack(Zip("missing", f => f.Remove("Bin/futureperfect_recomp.exe"), null, false), Path.Combine(temp, "missing-payload"), "v1.1.0")));
            Test("install and rollback preserve all user data", () => {
                string root = NewRoot("install");
                var changes = Core.Unpack(Path.Combine(temp, "valid.zip"), Path.Combine(temp, "install-payload"), "v1.1.0");
                Core.Apply(root, changes, null); Assert(Core.InstalledVersion(root) == "v1.1.0"); Personal(root);
                Assert(File.ReadAllText(Path.Combine(root, Core.LauncherName)) == "new launcher");
                Assert(Core.CanRollback(root)); Core.Rollback(root);
                Assert(Core.InstalledVersion(root) == "v1.0.0"); Assert(File.ReadAllText(Path.Combine(root, Core.LauncherName)) == "old launcher");
                Assert(!File.Exists(Path.Combine(root, "Launch.ps1"))); Personal(root);
            });
            Test("failed install restores previous files", () => {
                string root = NewRoot("failure");
                var changes = Core.Unpack(Path.Combine(temp, "valid.zip"), Path.Combine(temp, "failure-payload"), "v1.1.0");
                Reject(() => Core.Apply(root, changes, i => { if (i == 2) throw new IOException("simulated failure"); }));
                Assert(Core.InstalledVersion(root) == "v1.0.0"); Assert(File.ReadAllText(Path.Combine(root, "Bin/futureperfect_recomp.exe")) == "old game");
                Assert(!File.Exists(Path.Combine(root, "Launch.ps1"))); Personal(root);
            });
            Test("interrupted install recovers on next start", () => {
                string root = NewRoot("crash");
                Put(root, ".launcher/transaction/files/Bin/futureperfect_recomp.exe", "old game");
                Put(root, ".launcher/transaction/journal.json", Json.Encode(new[] { new Original { Name = "Bin/futureperfect_recomp.exe", Existed = true }, new Original { Name = "Launch.ps1", Existed = false } }));
                Put(root, "Bin/futureperfect_recomp.exe", "partial"); Put(root, "Launch.ps1", "new");
                Assert(Core.Recover(root)); Assert(!Core.Recover(root));
                Assert(File.ReadAllText(Path.Combine(root, "Bin/futureperfect_recomp.exe")) == "old game");
                Assert(!File.Exists(Path.Combine(root, "Launch.ps1"))); Personal(root);
            });
            Test("missing recovery backup blocks repair", () => {
                string root = NewRoot("broken-recovery");
                Put(root, ".launcher/transaction/journal.json", Json.Encode(new[] { new Original { Name = "Bin/futureperfect_recomp.exe", Existed = true } }));
                Reject(() => Core.Recover(root)); Assert(Directory.Exists(Path.Combine(root, ".launcher/transaction"))); Personal(root);
            });
            Test("fresh install seeds controls only once", () => {
                string root = Path.Combine(temp, "fresh"); Directory.CreateDirectory(root);
                var changes = Core.Unpack(Path.Combine(temp, "valid.zip"), Path.Combine(temp, "fresh-payload"), "v1.1.0");
                Core.Apply(root, changes, null); Assert(File.ReadAllText(Path.Combine(root, "Settings/pc-controls.json")) == "defaults");
                Put(root, "Settings/pc-controls.json", "changed"); Core.Apply(root, changes, null);
                Assert(File.ReadAllText(Path.Combine(root, "Settings/pc-controls.json")) == "changed");
            });
            Test("changed staged payload rejected", () => {
                string root = NewRoot("tampered-stage"); string pending = Path.Combine(root, ".launcher/pending"); Directory.CreateDirectory(pending);
                Core.Unpack(Path.Combine(temp, "valid.zip"), Path.Combine(pending, "payload"), "v1.1.0");
                Put(pending, "release.json", "{\"version\":\"v1.1.0\"}"); Put(pending, "payload/Launch.ps1", "tampered");
                Reject(() => Core.InstallPending(root)); Assert(Core.InstalledVersion(root) == "v1.0.0"); Personal(root);
            });
            Test("separate helper process applies and reports completion", () => {
                string root = NewRoot("helper install with spaces"); string pending = Path.Combine(root, ".launcher/pending"); Directory.CreateDirectory(pending);
                Core.Unpack(Path.Combine(temp, "valid.zip"), Path.Combine(pending, "payload"), "v1.1.0");
                Put(pending, "release.json", "{\"version\":\"v1.1.0\"}");
                string exe = System.Reflection.Assembly.GetExecutingAssembly().Location;
                using (var process = Process.Start(new ProcessStartInfo(exe, "--helper " + LauncherWindow.Quote(root)) { UseShellExecute = false, CreateNoWindow = true }))
                {
                    Assert(process.WaitForExit(15000)); Assert(process.ExitCode == 0);
                }
                Assert(Core.InstalledVersion(root) == "v1.1.0");
                Assert(File.ReadAllText(Path.Combine(root, ".launcher/last-result.txt")).Contains("successfully"));
                Assert(!Directory.Exists(pending)); Personal(root);
            });
            Test("ISO importer rejects truncated images without writing Data", () => {
                string iso = Path.Combine(temp, "truncated.iso"); File.WriteAllText(iso, "not an ISO");
                Reject(() => TsfpImport.ImportIso(iso, Path.Combine(temp, "unwritten-data")));
                Assert(!Directory.Exists(Path.Combine(temp, "unwritten-data")));
            });
            Test("checksum download accepted", () => DownloadTest("download-good", "bytes", "bytes", 5, false));
            Test("bad download checksum rejected", () => DownloadTest("download-bad", "bytes", "other", 5, true));
            Test("truncated download rejected", () => DownloadTest("download-short", "bytes", "bytes", 6, true));
            Test("oversized download rejected", () => DownloadTest("download-long", "bytes", "bytes", 4, true));
            Test("checksum sidecar fallback", () => {
                var release = ReleaseClient.Select(Json.Encode(Release("v1.0.0", false, false)), false);
                release.Digest = ""; release.ChecksumUrl = release.AssetUrl + ".sha256"; release.Size = 5;
                var handler = new Handler { Reply = r => new HttpResponseMessage(HttpStatusCode.OK) { Content = r.RequestUri.AbsolutePath.EndsWith(".sha256") ?
                    (HttpContent)new StringContent(Hash("bytes") + "  " + release.AssetName + "\n") : new StringContent("bytes") } };
                using (var client = new ReleaseClient(handler)) client.Download(release, Path.Combine(temp, "sidecar.zip"), new Progress(), CancellationToken.None).GetAwaiter().GetResult();
            });
            Test("GitHub rate limit is recoverable", () => {
                using (var client = new ReleaseClient(new Handler { Reply = r => new HttpResponseMessage(HttpStatusCode.Forbidden) }))
                    Reject(() => client.Check(false, CancellationToken.None).GetAwaiter().GetResult());
            });
            Test("cleanup cannot target user data", () => Reject(() => Core.RemoveOwned(NewRoot("cleanup"), "Data")));
            SelfUpdateTests();
            GamePruningTests();
            Console.WriteLine("PASS: " + passed + " isolated launcher regressions.");
            return 0;
        }
        catch (Exception ex) { Console.Error.WriteLine(ex); return 1; }
        finally
        {
            // The only recursive delete is this explicitly resolved, test-owned path.
            string baseDir = Path.GetFullPath(Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location)) + Path.DirectorySeparatorChar;
            if (Path.GetFullPath(temp).StartsWith(baseDir, StringComparison.OrdinalIgnoreCase) && Path.GetFileName(temp) == "updater-tests") Directory.Delete(temp, true);
        }
    }
    static string GameRoot(string name)
    {
        string root = NewRoot(name);
        Put(root, "Launch.ps1", "old launch");
        var files = new Dictionary<string, string> { { "Bin/futureperfect_recomp.exe", Hash("old game") },
            { "Launch.ps1", Hash("old launch") }, { "Settings/pc-controls.json", Hash("default controls") },
            { Core.LauncherName, Hash("packaged launcher") } };
        Put(root, "BUILD.json", Json.Encode(new { version = "v1.0.0", files = files }));
        Put(root, ".launcher/rollback/files/BUILD.json", "{\"version\":\"v0.9.0\"}");
        Put(root, ".launcher/rollback/files/Bin/futureperfect_recomp.exe", "older game");
        Put(root, ".launcher/rollback/journal.json", "[]");
        Put(root, "OriginalGame.ISO", "personal ISO");
        return root;
    }
    static void GameCleanupTests()
    {
        Test("game backup waits for a visible game and clean exit, then preserves personal files and ISO", () => {
            string root = GameRoot("game cleanup success"); string build = Core.GameBuildHash(root);
            Put(root, ".launcher/pending/release.zip", "incomplete download");
            Core.CleanupGameAfterStartup(root); Assert(Core.CanRollback(root));
            Core.FinishGameSession(root, build, false, 0); Assert(Core.CanRollback(root));
            Core.FinishGameSession(root, build, true, 1); Assert(Core.CanRollback(root));
            Assert(!File.Exists(Path.Combine(root, ".launcher/game-confirmed.json")));
            Core.FinishGameSession(root, build, true, 0);
            Assert(!Directory.Exists(Path.Combine(root, ".launcher/rollback")) && !Directory.Exists(Path.Combine(root, ".launcher/game-obsolete")));
            Assert(!Directory.Exists(Path.Combine(root, ".launcher/pending")) && !Core.CanRollback(root));
            Assert(File.Exists(Path.Combine(root, ".launcher/game-confirmed.json")));
            Assert(File.ReadAllText(Path.Combine(root, "OriginalGame.ISO")) == "personal ISO"); Personal(root);
            Assert(File.ReadAllText(Path.Combine(root, "Bin/futureperfect_recomp.exe")) == "old game");
        });
        Test("a different or damaged installed build keeps the game backup", () => {
            string root = GameRoot("game cleanup damaged"); string build = Core.GameBuildHash(root);
            Put(root, "Launch.ps1", "changed"); Core.FinishGameSession(root, build, true, 0); Assert(Core.CanRollback(root));
            Put(root, "Launch.ps1", "old launch");
            Put(root, "BUILD.json", File.ReadAllText(Path.Combine(root, "BUILD.json")) + "\n");
            Core.FinishGameSession(root, build, true, 0); Assert(Core.CanRollback(root));
            Assert(!File.Exists(Path.Combine(root, ".launcher/game-confirmed.json"))); Personal(root);
        });
        Test("confirmation never carries across game versions", () => {
            string root = GameRoot("game cleanup fingerprint");
            Core.FinishGameSession(root, Core.GameBuildHash(root), true, 0);
            Put(root, ".launcher/rollback/files/BUILD.json", "backup"); Put(root, ".launcher/rollback/journal.json", "[]");
            Put(root, "BUILD.json", File.ReadAllText(Path.Combine(root, "BUILD.json")).Replace("v1.0.0", "v1.1.0"));
            Core.CleanupGameAfterStartup(root); Assert(Core.CanRollback(root));
        });
        Test("unfinished game or launcher transactions block game cleanup", () => {
            string root = GameRoot("game cleanup transaction");
            foreach (string name in new[] { "transaction", "self-transaction" }) {
                Put(root, ".launcher/" + name + "/keep", "recovery");
                Core.FinishGameSession(root, Core.GameBuildHash(root), true, 0); Assert(Core.CanRollback(root));
                Core.RemoveOwned(root, ".launcher/" + name);
            }
        });
        Test("newer and unreadable game staging remains available", () => {
            string root = GameRoot("game cleanup newer stage");
            foreach (string marker in new[] { "{\"version\":\"v1.0.1\"}", "invalid JSON" }) {
                Put(root, ".launcher/pending/release.json", marker); Put(root, ".launcher/pending/release.zip", "keep");
                Core.FinishGameSession(root, Core.GameBuildHash(root), true, 0);
                Assert(File.Exists(Path.Combine(root, ".launcher/pending/release.zip")));
            }
        });
        Test("obsolete ready game staging is removed after confirmation", () => {
            string root = GameRoot("game cleanup old stage"); Core.FinishGameSession(root, Core.GameBuildHash(root), true, 0);
            foreach (string version in new[] { "v0.9.0", "v1.0.0-alpha.10", "v1.0.0" }) {
                Put(root, ".launcher/pending/release.json", Json.Encode(new { version = version }));
                Core.CleanupGameAfterStartup(root); Assert(!Directory.Exists(Path.Combine(root, ".launcher/pending")));
            }
        });
        Test("ISO and personal backups inside managed game folders prevent deletion", () => {
            foreach (string name in new[] { "rollback", "pending", "game-obsolete" }) {
                string root = GameRoot("game cleanup ISO " + name);
                Put(root, ".launcher/" + name + "/MyGame.IsO", "keep ISO");
                Core.FinishGameSession(root, Core.GameBuildHash(root), true, 0);
                Assert(File.ReadAllText(Path.Combine(root, ".launcher/" + name + "/MyGame.IsO")) == "keep ISO"); Personal(root);
            }
            string settings = GameRoot("game cleanup personal backup");
            Put(settings, ".launcher/rollback/files/Settings/runtime.conf", "personal backup");
            Core.FinishGameSession(settings, Core.GameBuildHash(settings), true, 0);
            Assert(File.ReadAllText(Path.Combine(settings, ".launcher/rollback/files/Settings/runtime.conf")) == "personal backup");
        });
        Test("locked old game files retry without offering a partial rollback", () => {
            string root = GameRoot("game cleanup lock");
            string file = Path.Combine(root, ".launcher/rollback/files/Bin/futureperfect_recomp.exe");
            using (var locked = File.Open(file, FileMode.Open, FileAccess.Read, FileShare.Read)) {
                Core.FinishGameSession(root, Core.GameBuildHash(root), true, 0);
                bool retired = Directory.Exists(Path.Combine(root, ".launcher/game-obsolete"));
                Assert(retired || Directory.Exists(Path.Combine(root, ".launcher/rollback")));
                if (retired) Assert(!Core.CanRollback(root));
            }
            Core.CleanupGameAfterStartup(root);
            Assert(!Directory.Exists(Path.Combine(root, ".launcher/rollback")) && !Directory.Exists(Path.Combine(root, ".launcher/game-obsolete"))); Personal(root);
        });
        Test("game staging cannot discard an assigned ISO", () => {
            string root = GameRoot("game cleanup retry ISO"); Put(root, ".launcher/pending/Game.ISO", "keep ISO");
            Reject(() => Core.DiscardGameStaging(root));
            Assert(File.ReadAllText(Path.Combine(root, ".launcher/pending/Game.ISO")) == "keep ISO");
        });
        Test("game cleanup refuses junctions without changing their target", () => {
            string root = GameRoot("game cleanup link"); string target = Path.Combine(temp, "game link target"); Directory.CreateDirectory(target); Put(target, "keep", "outside");
            string link = Core.PathIn(root, ".launcher/game-obsolete");
            try {
                using (var command = Process.Start(new ProcessStartInfo("cmd.exe", "/c mklink /J " + SelfUpdate.Quote(link) + " " + SelfUpdate.Quote(target)) { UseShellExecute = false, CreateNoWindow = true }))
                    Assert(command.WaitForExit(5000) && command.ExitCode == 0);
                Core.FinishGameSession(root, Core.GameBuildHash(root), true, 0);
                Assert(File.ReadAllText(Path.Combine(target, "keep")) == "outside" && Directory.Exists(link));
            } finally {
                if (Directory.Exists(link)) {
                    Assert(Path.GetDirectoryName(link) == Path.Combine(root, ".launcher") && (File.GetAttributes(link) & FileAttributes.ReparsePoint) != 0);
                    Directory.Delete(link);
                }
            }
        });
    }
    static void GamePruningTests()
    {
        Test("game update prunes unchanged retired files transactionally and rollback restores them", () => {
            string root = GameRoot("game pruning upgrade");
            var files = (Dictionary<string, object>)Json.Object(File.ReadAllText(Path.Combine(root, "BUILD.json")))["files"];
            foreach (string name in new[] { "Bin/retired.dll", "Source/old-source.zip", "Support/modified.txt" }) {
                Put(root, name, "old package file"); files[name] = Hash("old package file");
            }
            Put(root, "Support/modified.txt", "user modification"); Put(root, "Support/untracked.txt", "user file");
            // A legacy manifest may vary filename casing on Windows.
            files.Remove(Core.LauncherName); files[Core.LauncherName.ToLowerInvariant()] = Hash("old launcher");
            Put(root, "BUILD.json", Json.Encode(new { version = "v1.0.0", files = files }));
            string pending = Path.Combine(root, ".launcher/pending"); Directory.CreateDirectory(pending);
            Core.Unpack(Zip("game pruning", f => f.Remove(Core.LauncherName), null, false), Path.Combine(pending, "payload"), "v1.1.0");
            Put(pending, "release.json", "{\"version\":\"v1.1.0\"}"); Core.InstallPending(root);
            Assert(!File.Exists(Path.Combine(root, "Bin/retired.dll")) && !File.Exists(Path.Combine(root, "Source/old-source.zip")));
            Assert(File.ReadAllText(Path.Combine(root, "Support/modified.txt")) == "user modification" && File.Exists(Path.Combine(root, "Support/untracked.txt")));
            Assert(File.Exists(Path.Combine(root, "OriginalGame.ISO"))); Personal(root);
            Assert(File.ReadAllText(Path.Combine(root, Core.LauncherName)) == "old launcher");
            Core.Rollback(root);
            Assert(File.ReadAllText(Path.Combine(root, "Bin/retired.dll")) == "old package file" && File.ReadAllText(Path.Combine(root, "Source/old-source.zip")) == "old package file");
            Assert(Core.InstalledVersion(root) == "v1.0.0"); Personal(root);
        });
        Test("a protected ISO in the previous snapshot blocks its replacement safely", () => {
            string root = GameRoot("game pruning protected snapshot"); Put(root, ".launcher/rollback/My.ISO", "keep ISO");
            string pending = Path.Combine(root, ".launcher/pending"); Directory.CreateDirectory(pending);
            Core.Unpack(Zip("game pruning ISO", null, null, false), Path.Combine(pending, "payload"), "v1.1.0");
            Put(pending, "release.json", "{\"version\":\"v1.1.0\"}"); Reject(() => Core.InstallPending(root));
            Assert(Core.InstalledVersion(root) == "v1.0.0" && File.ReadAllText(Path.Combine(root, "Bin/futureperfect_recomp.exe")) == "old game");
            Assert(File.ReadAllText(Path.Combine(root, ".launcher/rollback/My.ISO")) == "keep ISO"); Personal(root);
        });
    }
    static void CleanupTests()
    {
        Test("successful startup removes old launcher files across repeated upgrades and preserves user files", () => {
            string root = NewRoot("cleanup repeated upgrades");
            Put(root, ".launcher/rollback/keep", "game rollback");
            Put(root, ".launcher/preferences.json", "preferences");
            Put(root, ".launcher/last-result.txt", "game result");
            Put(root, ".launcher/self-last-result.txt", "launcher result");
            Put(root, ".launcher/unrelated/keep", "unknown files");
            Put(root, "TimeSplitters-Launcher-v0.1.0-windows-x64.zip", "manual download");
            for (int i = 1; i <= 3; i++) {
                PutSelf(root, "1.0." + i);
                Put(root, ".launcher/self-rollback/files/" + Core.LauncherName, "older launcher");
                Put(root, ".launcher/self-pending/launcher.zip", "partial download");
                Put(root, ".launcher/self-helper.exe", "old bootstrap helper");
                Put(root, ".launcher/update-helper.exe", "old application helper");
                SelfUpdate.CleanupAfterStartup(root);
                foreach (string name in new[] { "self-rollback", "self-pending", "self-helper.exe", "update-helper.exe" })
                    Assert(!File.Exists(Path.Combine(root, ".launcher", name)) && !Directory.Exists(Path.Combine(root, ".launcher", name)));
                Assert(SelfUpdate.Complete(root)); Personal(root);
                Assert(Core.InstalledVersion(root) == "v1.0.0");
            }
            Assert(File.ReadAllText(Path.Combine(root, ".launcher/rollback/keep")) == "game rollback");
            Assert(File.ReadAllText(Path.Combine(root, ".launcher/preferences.json")) == "preferences");
            Assert(File.Exists(Path.Combine(root, ".launcher/last-result.txt")) && File.Exists(Path.Combine(root, ".launcher/self-last-result.txt")));
            Assert(File.Exists(Path.Combine(root, ".launcher/unrelated/keep")) && File.Exists(Path.Combine(root, "TimeSplitters-Launcher-v0.1.0-windows-x64.zip")));
        });
        Test("unverified launcher retains recovery and staging files", () => {
            string root = NewRoot("cleanup incomplete"); PutSelf(root, "1.0.0"); Put(root, Core.AppPath, "corrupt");
            foreach (string name in new[] { "self-rollback/keep", "self-pending/launcher.zip", "self-helper.exe", "update-helper.exe" }) Put(root, ".launcher/" + name, "keep");
            SelfUpdate.CleanupAfterStartup(root);
            foreach (string name in new[] { "self-rollback/keep", "self-pending/launcher.zip", "self-helper.exe", "update-helper.exe" }) Assert(File.Exists(Path.Combine(root, ".launcher", name)));
        });
        Test("unfinished launcher transaction prevents cleanup until recovery", () => {
            string root = NewRoot("cleanup interrupted"); PutSelf(root, "1.0.0");
            Put(root, ".launcher/self-transaction/files/" + Core.LauncherName, "recovery copy");
            Put(root, ".launcher/self-rollback/keep", "backup"); Put(root, ".launcher/self-helper.exe", "helper");
            SelfUpdate.CleanupAfterStartup(root);
            Assert(File.Exists(Path.Combine(root, ".launcher/self-transaction/files/" + Core.LauncherName)));
            Assert(File.Exists(Path.Combine(root, ".launcher/self-rollback/keep")) && File.Exists(Path.Combine(root, ".launcher/self-helper.exe")));
        });
        Test("newer staged launcher and its helper are preserved", () => {
            string root = NewRoot("cleanup newer pending"); PutSelf(root, "1.0.0");
            Put(root, ".launcher/self-pending/release.json", "{\"version\":\"1.0.1\"}");
            Put(root, ".launcher/self-pending/payload/keep", "new update"); Put(root, ".launcher/self-helper.exe", "helper");
            SelfUpdate.CleanupAfterStartup(root);
            Assert(File.Exists(Path.Combine(root, ".launcher/self-pending/payload/keep")) && File.Exists(Path.Combine(root, ".launcher/self-helper.exe")));
        });
        Test("already installed or older staging is removed using semantic versions", () => {
            string root = NewRoot("cleanup old pending"); PutSelf(root, "1.10.0");
            foreach (string version in new[] { "1.9.0", "1.10.0-beta.1", "1.10.0" }) {
                Put(root, ".launcher/self-pending/release.json", Json.Encode(new { version = version }));
                Put(root, ".launcher/self-pending/payload/keep", "old payload");
                SelfUpdate.CleanupAfterStartup(root); Assert(!Directory.Exists(Path.Combine(root, ".launcher/self-pending")));
            }
        });
        Test("unreadable staging marker is retained for diagnosis", () => {
            string root = NewRoot("cleanup unknown pending"); PutSelf(root, "1.0.0");
            Put(root, ".launcher/self-pending/release.json", "invalid JSON"); Put(root, ".launcher/self-helper.exe", "helper");
            SelfUpdate.CleanupAfterStartup(root);
            Assert(File.Exists(Path.Combine(root, ".launcher/self-pending/release.json")) && File.Exists(Path.Combine(root, ".launcher/self-helper.exe")));
        });
        Test("locked cleanup files are retried without failing startup", () => {
            string root = NewRoot("cleanup locked"); PutSelf(root, "1.0.0");
            Put(root, ".launcher/self-rollback/locked", "backup"); Put(root, ".launcher/self-helper.exe", "helper");
            using (var backup = File.Open(Path.Combine(root, ".launcher/self-rollback/locked"), FileMode.Open, FileAccess.Read, FileShare.Read))
            using (var helper = File.Open(Path.Combine(root, ".launcher/self-helper.exe"), FileMode.Open, FileAccess.Read, FileShare.Read)) {
                SelfUpdate.CleanupAfterStartup(root);
                Assert(File.Exists(Path.Combine(root, ".launcher/self-rollback/locked")) && File.Exists(Path.Combine(root, ".launcher/self-helper.exe")));
                Assert(File.ReadAllText(Path.Combine(root, ".launcher/launcher.log")).Contains("cleanup deferred"));
            }
            SelfUpdate.CleanupAfterStartup(root);
            Assert(!Directory.Exists(Path.Combine(root, ".launcher/self-rollback")) && !File.Exists(Path.Combine(root, ".launcher/self-helper.exe")));
        });
        Test("pending game updates and transactions keep their helper and rollback", () => {
            string root = NewRoot("cleanup game work"); PutSelf(root, "1.0.0"); Put(root, ".launcher/rollback/keep", "game rollback");
            foreach (string folder in new[] { "pending", "transaction" }) {
                Put(root, ".launcher/" + folder + "/keep", "game work"); Put(root, ".launcher/update-helper.exe", "game helper");
                SelfUpdate.CleanupAfterStartup(root);
                Assert(File.Exists(Path.Combine(root, ".launcher/update-helper.exe")) && File.Exists(Path.Combine(root, ".launcher/" + folder + "/keep")));
                Core.RemoveOwned(root, ".launcher/" + folder);
            }
            Assert(File.ReadAllText(Path.Combine(root, ".launcher/rollback/keep")) == "game rollback");
        });
        Test("cleanup refuses junctions without touching their target", () => {
            string root = NewRoot("cleanup linked"); PutSelf(root, "1.0.0");
            string target = Path.Combine(temp, "junction target"); Directory.CreateDirectory(target); Put(target, "keep", "outside cleanup");
            string link = Core.PathIn(root, ".launcher/self-rollback"); Directory.CreateDirectory(Path.GetDirectoryName(link));
            try {
                using (var command = Process.Start(new ProcessStartInfo("cmd.exe", "/c mklink /J " + SelfUpdate.Quote(link) + " " + SelfUpdate.Quote(target)) {
                    UseShellExecute = false, CreateNoWindow = true, RedirectStandardOutput = true, RedirectStandardError = true }))
                    Assert(command.WaitForExit(5000) && command.ExitCode == 0);
                SelfUpdate.CleanupAfterStartup(root);
                Assert(File.ReadAllText(Path.Combine(target, "keep")) == "outside cleanup" && Directory.Exists(link));
            } finally {
                if (Directory.Exists(link)) {
                    Assert(Path.GetDirectoryName(link) == Path.Combine(root, ".launcher") && (File.GetAttributes(link) & FileAttributes.ReparsePoint) != 0);
                    Directory.Delete(link); // Remove only this test-created junction, never its target.
                }
            }
        });
    }
    static void SelfUpdateTests()
    {
        Test("launcher version independent of game tag", () => {
            var json = Json.Encode(new[] { SelfRelease("1.1.0", false, false, 100, ""), SelfRelease("1.2.0-beta.1", true, false, 100, ""), SelfRelease("9.0.0", false, true, 100, "") });
            Assert(ReleaseClient.SelectLauncher(json, false).Package.Tag == "1.1.0");
            Assert(ReleaseClient.SelectLauncher(json, true).Package.Tag == "1.2.0-beta.1");
        });
        Test("game releases do not become launcher updates", () => Assert(ReleaseClient.SelectLauncher(Json.Encode(new[] { Release("v9.0.0", false, false) }), false) == null));
        Test("newer game-only release cannot hide launcher release", () => {
            var json = Json.Encode(new object[] { Release("v99.0.0", false, false), SelfRelease("1.1.0", false, false, 100, "") });
            Assert(ReleaseClient.SelectLauncher(json, false).Package.Tag == "1.1.0");
        });
        Test("launcher-only release cannot hide a stable game release", () => {
            var json = Json.Encode(new object[] { SelfRelease("99.0.0", false, false, 100, ""), Release("v1.0.0", false, false) });
            Assert(ReleaseClient.Select(json, false).Tag == "v1.0.0");
        });
        Test("launcher metadata verifies independent version", () => {
            var candidate = ReleaseClient.SelectLauncher(Json.Encode(new[] { SelfRelease("1.1.0", false, false, 100, "") }), false);
            Assert(ReleaseClient.VerifyLauncherMetadata(candidate, SelfMetadata("1.1.0", 100, Hash("zip"))).Digest == "sha256:" + Hash("zip"));
        });
        Test("launcher metadata cannot substitute version", () => {
            var candidate = ReleaseClient.SelectLauncher(Json.Encode(new[] { SelfRelease("1.1.0", false, false, 100, "") }), false);
            Reject(() => ReleaseClient.VerifyLauncherMetadata(candidate, SelfMetadata("9.0.0", 100, Hash("zip"))));
        });
        Test("launcher metadata must match GitHub checksum", () => {
            var candidate = ReleaseClient.SelectLauncher(Json.Encode(new[] { SelfRelease("1.1.0", false, false, 100, "sha256:" + Hash("other")) }), false);
            Reject(() => ReleaseClient.VerifyLauncherMetadata(candidate, SelfMetadata("1.1.0", 100, Hash("zip"))));
        });
        Test("launcher check has bounded timeout even if network ignores cancellation", () => {
            var watch = Stopwatch.StartNew();
            using (var client = new ReleaseClient(new SlowHandler()))
            {
                try { SelfUpdate.CheckWithTimeout(client, false, TimeSpan.FromMilliseconds(20), CancellationToken.None).GetAwaiter().GetResult(); throw new Exception("Expected timeout"); }
                catch (TimeoutException) { Assert(watch.ElapsedMilliseconds < 350); }
            }
        });
        Test("continue offline cancels launcher check", () => {
            using (var client = new ReleaseClient(new SlowHandler())) using (var cancel = new CancellationTokenSource(20))
            {
                try { SelfUpdate.CheckWithTimeout(client, false, TimeSpan.FromSeconds(5), cancel.Token).GetAwaiter().GetResult(); throw new Exception("Expected cancellation"); }
                catch (OperationCanceledException) { }
            }
        });
        Test("launcher no-update result needs no install", () => {
            using (var client = new ReleaseClient(new Handler { Reply = r => new HttpResponseMessage(HttpStatusCode.OK) { Content = new StringContent("[]") } }))
                Assert(SelfUpdate.CheckWithTimeout(client, false, TimeSpan.FromSeconds(1), CancellationToken.None).GetAwaiter().GetResult() == null);
        });
        string zip = SelfZip("self-valid", "1.1.0", null, false);
        foreach (string failure in new[] { "checksum", "archive", "cancelled" })
            Test("failed launcher preparation removes staging: " + failure, () => {
                string root = NewRoot("self preparation " + failure); PutSelf(root, "1.0.0");
                byte[] bytes = failure == "archive" ? Encoding.UTF8.GetBytes("not a ZIP") : File.ReadAllBytes(zip);
                string hash = failure == "checksum" ? new string('0', 64) : HashBytes(bytes);
                var release = new Release { Tag = "1.1.0", Size = bytes.Length, Digest = "sha256:" + hash,
                    AssetUrl = "https://github.com/" + ReleaseClient.Repository + "/releases/download/launcher-v1.1.0/launcher.zip" };
                using (var cancel = new CancellationTokenSource())
                using (var client = new ReleaseClient(new Handler { Reply = r => new HttpResponseMessage(HttpStatusCode.OK) { Content = new ByteArrayContent(bytes) } })) {
                    if (failure == "cancelled") cancel.Cancel();
                    bool failed = false;
                    try { SelfUpdate.Prepare(root, release, client, new Progress(), cancel.Token).GetAwaiter().GetResult(); }
                    catch (Exception ex) {
                        failed = !ex.Message.Contains("Close the game") && (failure == "cancelled" ? ex is OperationCanceledException :
                            failure == "checksum" ? ex.Message.Contains("Checksum mismatch") : ex is InvalidDataException);
                    }
                    Assert(failed && !Directory.Exists(Path.Combine(root, ".launcher/self-pending")) && SelfUpdate.Complete(root)); Personal(root);
                }
            });
        Test("launcher-only ZIP verifies exactly three files", () => Assert(SelfUpdate.Unpack(zip, Path.Combine(temp, "self-valid-payload"), "1.1.0").Count == 3));
        Test("launcher archive rejects game executable", () => Reject(() => SelfUpdate.Unpack(SelfZip("self-game", "1.1.0", "Bin/futureperfect_recomp.exe", false), Path.Combine(temp, "self-game-payload"), "1.1.0")));
        Test("launcher archive rejects personal settings", () => Reject(() => SelfUpdate.Unpack(SelfZip("self-settings", "1.1.0", "Settings/pc-controls.json", false), Path.Combine(temp, "self-settings-payload"), "1.1.0")));
        Test("launcher archive rejects traversal", () => Reject(() => SelfUpdate.Unpack(SelfZip("self-traverse", "1.1.0", "../outside.exe", false), Path.Combine(temp, "self-traverse-payload"), "1.1.0")));
        Test("launcher archive rejects case duplicate", () => Reject(() => SelfUpdate.Unpack(SelfZip("self-duplicate", "1.1.0", "bin/TIMESPLITTERS.APP.exe", false), Path.Combine(temp, "self-duplicate-payload"), "1.1.0")));
        Test("launcher archive rejects wrong version", () => Reject(() => SelfUpdate.Unpack(zip, Path.Combine(temp, "self-version-payload"), "9.0.0")));
        Test("launcher archive rejects changed file", () => Reject(() => SelfUpdate.Unpack(SelfZip("self-hash", "1.1.0", null, true), Path.Combine(temp, "self-hash-payload"), "1.1.0")));
        Test("self-update rechecks staged files", () => {
            string root = NewRoot("self-tampered"); PutSelf(root, "1.0.0");
            string pending = Path.Combine(root, ".launcher/self-pending"); Directory.CreateDirectory(pending);
            SelfUpdate.Unpack(zip, Path.Combine(pending, "payload"), "1.1.0"); Put(pending, "release.json", "{\"version\":\"1.1.0\"}");
            Put(pending, "payload/" + Core.AppPath, "tampered"); Reject(() => SelfUpdate.InstallPending(root));
            Assert(SelfUpdate.InstalledVersion(root) == "1.0.0" && SelfUpdate.Complete(root)); Personal(root);
        });
        Test("self-update helper replaces launcher only and preserves game rollback", () => {
            string root = NewRoot("self handoff with spaces"); PutSelf(root, "1.0.0");
            Put(root, ".launcher/rollback/keep.txt", "game rollback");
            byte[] bytes = File.ReadAllBytes(zip); long size = bytes.Length; string hash = Core.Hash(zip);
            var handler = new Handler { Reply = request => new HttpResponseMessage(HttpStatusCode.OK) { Content =
                request.RequestUri.Host == "api.github.com" ? (HttpContent)new StringContent(Json.Encode(new[] { SelfRelease("1.1.0", false, false, size, "sha256:" + hash) })) :
                request.RequestUri.AbsolutePath.EndsWith("launcher-update.json") ? new StringContent(SelfMetadata("1.1.0", size, hash)) : new ByteArrayContent(bytes) } };
            using (var client = new ReleaseClient(handler))
            {
                var release = client.CheckLauncher(false, CancellationToken.None).GetAwaiter().GetResult();
                Assert(SelfUpdate.Prepare(root, release, client, new Progress(), CancellationToken.None).GetAwaiter().GetResult());
                Assert(SelfUpdate.InstalledVersion(root) == "1.0.0");
            }
            string exe = System.Reflection.Assembly.GetExecutingAssembly().Location;
            using (var process = Process.Start(new ProcessStartInfo(exe, "--self-helper " + SelfUpdate.Quote(root)) { UseShellExecute = false, CreateNoWindow = true }))
            { Assert(process.WaitForExit(15000)); Assert(process.ExitCode == 0); }
            Assert(SelfUpdate.InstalledVersion(root) == "1.1.0" && SelfUpdate.Complete(root));
            Assert(Core.InstalledVersion(root) == "v1.0.0"); Personal(root);
            Assert(File.ReadAllText(Path.Combine(root, ".launcher/rollback/keep.txt")) == "game rollback");
            Assert(File.ReadAllText(Path.Combine(root, ".launcher/self-rollback/files/" + Core.LauncherName)) == "bootstrap 1.0.0");
        });
        Test("already current launcher avoids download", () => {
            string root = NewRoot("self-current"); PutSelf(root, "1.1.0");
            using (var client = new ReleaseClient(new Handler { Reply = r => { throw new Exception("Unexpected download"); } }))
                Assert(!SelfUpdate.Prepare(root, new Release { Tag = "1.1.0" }, client, new Progress(), CancellationToken.None).GetAwaiter().GetResult());
        });
        Test("launcher never downgrades across channels", () => {
            string root = NewRoot("self-no-downgrade"); PutSelf(root, "2.0.0");
            using (var client = new ReleaseClient(new Handler { Reply = r => { throw new Exception("Unexpected download"); } }))
                Assert(!SelfUpdate.Prepare(root, new Release { Tag = "1.1.0" }, client, new Progress(), CancellationToken.None).GetAwaiter().GetResult());
        });
        Test("failed launcher install restores previous launcher", () => {
            string root = NewRoot("self-failure"); PutSelf(root, "1.0.0");
            var changes = SelfUpdate.Unpack(zip, Path.Combine(temp, "self-failure-payload"), "1.1.0");
            Reject(() => Core.ApplyLauncher(root, changes, i => { if (i == 1) throw new IOException("injected self failure"); }));
            Assert(SelfUpdate.InstalledVersion(root) == "1.0.0" && SelfUpdate.Complete(root)); Personal(root);
        });
        Test("self transaction cannot modify game files", () => {
            string root = NewRoot("self-protected");
            Reject(() => Core.ApplyLauncher(root, new List<Change> { new Change { Name = "BUILD.json", Source = null } }, null));
            Assert(Core.InstalledVersion(root) == "v1.0.0"); Personal(root);
        });
        Test("interrupted self-update recovers independently", () => {
            string root = NewRoot("self-crash"); PutSelf(root, "1.0.0");
            Put(root, ".launcher/self-transaction/files/" + Core.LauncherName, "bootstrap 1.0.0");
            Put(root, ".launcher/self-transaction/journal.json", Json.Encode(new[] { new Original { Name = Core.LauncherName, Existed = true } }));
            Put(root, Core.LauncherName, "partial");
            Assert(!Core.Recover(root)); Assert(Core.RecoverLauncher(root)); Assert(SelfUpdate.Complete(root)); Personal(root);
        });
        Test("game update and rollback preserve newer launcher", () => {
            string root = NewRoot("game-with-new-launcher"); PutSelf(root, "9.0.0");
            string pending = Path.Combine(root, ".launcher/pending"); Directory.CreateDirectory(pending);
            Core.Unpack(Path.Combine(temp, "valid.zip"), Path.Combine(pending, "payload"), "v1.1.0");
            Put(pending, "release.json", "{\"version\":\"v1.1.0\"}");
            Core.InstallPending(root); Assert(Core.InstalledVersion(root) == "v1.1.0"); Assert(SelfUpdate.Complete(root));
            Core.Rollback(root); Assert(Core.InstalledVersion(root) == "v1.0.0");
            Assert(SelfUpdate.InstalledVersion(root) == "9.0.0" && SelfUpdate.Complete(root)); Personal(root);
        });
        Test("explicit game version switching downgrades or reinstalls while keeping the launcher and personal files", () => {
            foreach(string from in new[]{"v2.0.0","v1.1.0"}) {
                string root=NewRoot("specific-game-"+from);PutSelf(root,"1.2.12");
                Put(root,"BUILD.json",Json.Encode(new {version=from}));
                Put(root,"disc.iso","personal ISO");
                string pending=Core.PathIn(root,".launcher/pending");Directory.CreateDirectory(pending);
                Core.Unpack(Path.Combine(temp,"valid.zip"),Path.Combine(pending,"payload"),"v1.1.0");
                string payload=Path.Combine(pending,"payload");PutSelf(payload,"99.0.0");
                var files=Files().Keys.Concat(new[]{Core.AppPath,Core.SelfManifest}).ToDictionary(n=>n,n=>Core.Hash(Core.PathIn(payload,n)));
                Put(payload,"BUILD.json",Json.Encode(new {version="v1.1.0",launcher_version="99.0.0",files=files}));
                Put(pending,"release.json",Json.Encode(new {version="v1.1.0",game_only=true}));
                Core.InstallPending(root);Assert(Core.InstalledVersion(root)=="v1.1.0" && SelfUpdate.InstalledVersion(root)=="1.2.12" && SelfUpdate.Complete(root));
                Assert(File.ReadAllText(Core.PathIn(root,"Bin/futureperfect_recomp.exe"))=="new game");Personal(root);Assert(File.ReadAllText(Core.PathIn(root,"disc.iso"))=="personal ISO");
                Core.Rollback(root);Assert(Core.InstalledVersion(root)==from && SelfUpdate.InstalledVersion(root)=="1.2.12" && SelfUpdate.Complete(root));Personal(root);
            }
        });
        Test("equal-version game launcher preserves artwork and repairs damaged copies", () => {
            foreach (bool damaged in new[] { false, true }) {
                string root = NewRoot("game-equal-launcher-" + damaged); PutSelf(root, "1.2.6");
                string original = File.ReadAllText(Path.Combine(root, Core.SelfManifest));
                if (damaged) Put(root, Core.AppPath, "damaged app");
                string pending = Path.Combine(root, ".launcher/pending"); Directory.CreateDirectory(pending);
                Core.Unpack(Path.Combine(temp, "valid.zip"), Path.Combine(pending, "payload"), "v1.1.0");
                string payload = Path.Combine(pending, "payload");
                PutSelf(payload, "1.2.6");
                Put(payload, Core.AppPath, "same-version fallback app");
                Put(payload, Core.SelfManifest, SelfManifest("1.2.6", "bootstrap 1.2.6", "same-version fallback app"));
                var files = Files().Keys.ToDictionary(n=>n,n=>Core.Hash(Core.PathIn(payload,n)));
                files[Core.AppPath]=Core.Hash(Core.PathIn(payload,Core.AppPath));
                files[Core.SelfManifest]=Core.Hash(Core.PathIn(payload,Core.SelfManifest));
                Put(payload,"BUILD.json",Json.Encode(new { version="v1.1.0",launcher_version="1.2.6",files=files }));
                Put(pending,"release.json","{\"version\":\"v1.1.0\"}");
                Core.InstallPending(root); Assert(SelfUpdate.Complete(root));
                Assert(File.ReadAllText(Core.PathIn(root,Core.AppPath)) == (damaged ? "same-version fallback app" : "app 1.2.6"));
                if(!damaged) Assert(File.ReadAllText(Core.PathIn(root,Core.SelfManifest))==original);
                Core.Rollback(root); Assert(SelfUpdate.Complete(root)); Personal(root);
            }
        });
        Test("produced launcher bundle verifies and installs in isolation", () => {
            string output = Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location);
            string metadataText = File.ReadAllText(Path.Combine(output, "launcher-update.json"));
            var metadata = Json.Object(metadataText);
            string archive = Path.Combine(output, Json.String(metadata, "asset_name"));
            long size = new FileInfo(archive).Length;
            Assert(Core.Hash(archive) == Json.String(metadata, "sha256"));
            var candidate = ReleaseClient.SelectLauncher(Json.Encode(new[] { SelfRelease(LauncherVersion.Value, false, false, size, "") }), false);
            ReleaseClient.VerifyLauncherMetadata(candidate, metadataText);
            var changes = SelfUpdate.Unpack(archive, Path.Combine(temp, "compiled-launcher-payload"), LauncherVersion.Value);
            string root = NewRoot("compiled-launcher-install");
            Core.ApplyLauncher(root, changes, null);
            Assert(SelfUpdate.Complete(root) && SelfUpdate.InstalledVersion(root) == LauncherVersion.Value);
            Assert(Core.InstalledVersion(root) == "v1.0.0"); Personal(root);
        });
    }
    static int VerifyPublishedRelease()
    {
        string root = Path.Combine(Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location), "published-release-check");
        if (Directory.Exists(root)) throw new Exception("A published-release-check folder already exists; inspect it first.");
        Directory.CreateDirectory(root);
        try
        {
            using (var client = new ReleaseClient()) using (var timeout = new CancellationTokenSource(TimeSpan.FromMinutes(10)))
            {
                var release = client.Check(true, timeout.Token).GetAwaiter().GetResult();
                Core.Require(release != null, "No compatible preview release is published.");
                Core.CheckSpace(root, release.Size * 5 + 256L * 1024 * 1024);
                string pending = Path.Combine(root, ".launcher/pending"); Directory.CreateDirectory(pending);
                string zip = Path.Combine(pending, "release.zip");
                Console.WriteLine("Downloading and verifying " + release.Tag + ": " + release.Size + " bytes.");
                client.Download(release, zip, new Progress(), timeout.Token).GetAwaiter().GetResult();
                Console.WriteLine("Archive SHA-256 verified.");
                var changes = Core.Unpack(zip, Path.Combine(pending, "payload"), release.Tag);
                Console.WriteLine("Manifest verified: " + changes.Count + " package files.");
                Put(pending, "release.json", Json.Encode(new { version = release.Tag }));
                File.Delete(zip);
                int result = Program.RunHelper("--apply", root, Int32.MaxValue, false);
                Assert(result == 0 && Core.InstalledVersion(root) == release.Tag);
                Assert(!Directory.Exists(Path.Combine(root, "Data")) && !Directory.Exists(Path.Combine(root, "Saves")));
                Console.WriteLine("Published runtime installed in isolation. No downloaded program was executed; no game data was copied.");
                return 0;
            }
        }
        finally
        {
            string baseDir = Path.GetFullPath(Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location)) + Path.DirectorySeparatorChar;
            if (Path.GetFullPath(root).StartsWith(baseDir, StringComparison.OrdinalIgnoreCase) && Path.GetFileName(root) == "published-release-check") Directory.Delete(root, true);
        }
    }
    static void DownloadTest(string name, string bytes, string expected, long size, bool reject)
    {
        var handler = new Handler { Reply = r => new HttpResponseMessage(HttpStatusCode.OK) { Content = new ByteArrayContent(Encoding.UTF8.GetBytes(bytes)) } };
        using (var client = new ReleaseClient(handler))
        {
            var release = new Release { Size = size, Digest = "sha256:" + Hash(expected), AssetUrl = "https://github.com/" + ReleaseClient.Repository + "/releases/download/v1.1.0/a.zip" };
            Action run = () => client.Download(release, Path.Combine(temp, name + ".zip"), new Progress(), CancellationToken.None).GetAwaiter().GetResult();
            if (reject) Reject(run); else { run(); Assert(File.ReadAllText(Path.Combine(temp, name + ".zip")) == bytes); }
        }
    }
}
