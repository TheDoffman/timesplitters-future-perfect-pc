using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using TimeSplitters.Launcher;

public static class LauncherRestartChecks
{
    static void Need(bool condition, string message) { if (!condition) throw new Exception(message); }
    static void Put(string root, string name, byte[] bytes)
    {
        string path = Core.PathIn(root, name);
        Directory.CreateDirectory(Path.GetDirectoryName(path)); File.WriteAllBytes(path, bytes);
    }
    static byte[] Bytes(string text) { return Encoding.UTF8.GetBytes(text); }
    public static void Stage(string root, string version, Dictionary<string, byte[]> extra = null)
    {
        string pending = Core.PathIn(root, ".launcher/pending");
        string payload = Path.Combine(pending, "payload");
        var files = new Dictionary<string, byte[]> {
            { LauncherPlatform.GameBinary, Bytes("game " + version) },
            { LauncherPlatform.LaunchScript, Bytes("launch " + version) },
            { "Settings/pc-controls.json", Bytes("package defaults") }
        };
        if (extra != null) foreach (var pair in extra) files[pair.Key] = pair.Value;
        foreach (var pair in files) Put(payload, pair.Key, pair.Value);
        var hashes = files.ToDictionary(p => p.Key, p => Core.Hash(Core.PathIn(payload, p.Key)));
        Core.Write(Path.Combine(payload, "BUILD.json"),
            Json.Encode(new { version, launcher_version = LauncherVersion.Value, files = hashes }));
        Core.Write(Path.Combine(pending, "release.json"), Json.Encode(new { version }));
    }
    static readonly string[] PersonalFiles = {
        "Data/keep", "Saves/keep", "Settings/pc-controls.json", "Settings/pc-display.ini",
        "Settings/runtime.conf", "Settings/pc-discord.ini", "Textures/keep.dds"
    };
    static void Personal(string root)
    {
        foreach (string path in PersonalFiles)
            Need(File.ReadAllText(Core.PathIn(root, path)) == "personal " + path, "Personal file changed: " + path);
    }
    public static void Run()
    {
        string root = Path.Combine(AppContext.BaseDirectory, "tsfp restart " + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            foreach (string path in PersonalFiles) Put(root, path, Bytes("personal " + path));
            Put(root, Core.LauncherName, Bytes("old launcher"));
            Stage(root, "v1.0.0"); Core.InstallPending(root);
            Stage(root, "v1.1.0", new Dictionary<string, byte[]> { { Core.LauncherName, Bytes("new launcher") } });
            int restarts = 0;
            Action restarted = () => {
                // A new process must be able to acquire the installation lock.
                var acquired = Task.Run(() => { using (LauncherPlatform.AcquireUpdateLock(root)) { } });
                Need(acquired.Wait(2000), "Restart happened before the installation lock was released");
                Personal(root); restarts++;
            };
            int result = UpdateRestart.ApplyGame(root, Int32.MaxValue, false, () => {
                Need(Core.InstalledVersion(root) == "v1.1.0", "Restart preceded the completed update");
                Need(File.ReadAllText(Core.PathIn(root, Core.LauncherName)) == "new launcher", "Old launcher reopened");
                Need(!Directory.Exists(Core.PathIn(root, ".launcher/pending")), "Restart preceded staging cleanup");
                restarted();
            });
            Need(result == 0 && restarts == 1, "Successful update did not restart once");
            Console.WriteLine("PASS shared restart: verified update replaces the launcher before reopening and preserves personal files");

            result = UpdateRestart.ApplyGame(root, Int32.MaxValue, true, restarted);
            Need(result == 0 && restarts == 2 && Core.InstalledVersion(root) == "v1.0.0", "Restore did not reopen the previous game");
            Console.WriteLine("PASS shared restart: restore reopens only after the transaction and lock release");

            Stage(root, "v1.2.0");
            Put(root, ".launcher/pending/payload/" + LauncherPlatform.GameBinary, Bytes("tampered"));
            result = UpdateRestart.ApplyGame(root, Int32.MaxValue, false, restarted);
            Need(result == 1 && restarts == 3 && Core.InstalledVersion(root) == "v1.0.0", "Failed update changed the game or failed to reopen");
            Need(File.ReadAllText(Core.PathIn(root, ".launcher/last-result.txt")).StartsWith("Update failed:"), "Failure result missing");
            Need(Directory.Exists(Core.PathIn(root, ".launcher/pending")), "Failure discarded recovery evidence");
            Console.WriteLine("PASS shared restart: rejected payload retains the installed game and reports failure without retrying");

            var start = LauncherPlatform.RestartInfo(root);
            Need(start.WorkingDirectory == root, "Restart changed the installation");
#if MACOS
            Need(start.FileName == "/usr/bin/open" && start.ArgumentList.SequenceEqual(new[] {
                "-n", Core.PathIn(root, "TimeSplitters Launcher.app"), "--args",
                "--skip-self-update", "--root", root }), "Mac restart lost the selected root or retry guard");
#else
            Need(start.FileName == Core.PathIn(root, Core.LauncherName) &&
                start.Arguments == "--after-self-update", "Windows restart lost the installed bootstrap or retry guard");
#endif
            Console.WriteLine("PASS shared restart: host command preserves spaced installation paths and skips one update check");
        }
        finally { Directory.Delete(root, true); }
    }
}
