using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using TimeSplitters.Launcher;

public static class LauncherRestartChecks
{
    sealed class Reports : IProgress<string> {
        public readonly List<string> Messages = new List<string>();
        public bool Throw;
        public void Report(string message) { Messages.Add(message);if(Throw)throw new IOException("injected observer failure"); }
    }
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
    static void ReuseChecks(string parent)
    {
        string root=Path.Combine(parent,"reuse");Directory.CreateDirectory(root);
        var same=new Dictionary<string,byte[]> {
            {"Support/reused.txt",Bytes("unchanged content")},
            {"Support/repair.txt",Bytes("correct content")},
            {LauncherPlatform.LaunchScript,Bytes("unchanged launch script")}
        };
        Stage(root,"v1.0.0",same);Core.InstallPending(root);
        string reused=Core.PathIn(root,"Support/reused.txt"),repair=Core.PathIn(root,"Support/repair.txt");
        var stamp=new DateTime(2001,1,1,0,0,0,DateTimeKind.Utc);File.SetLastWriteTimeUtc(reused,stamp);
        File.WriteAllText(repair,"damaged installed content");
#if MACOS || LINUX
        string script=Core.PathIn(root,LauncherPlatform.LaunchScript);
        File.SetUnixFileMode(script,UnixFileMode.UserRead|UnixFileMode.UserWrite);
#endif
        Stage(root,"v1.1.0",same);var progress=new Reports();Core.InstallPending(root,progress);
        Need(File.GetLastWriteTimeUtc(reused)==stamp,"Identical file was rewritten");
        Need(!File.Exists(Core.PathIn(root,".launcher/rollback/files/Support/reused.txt")),"Identical file was unnecessarily backed up");
        Need(File.ReadAllText(repair)=="correct content","Damaged installed file was reused");
        Need(File.ReadAllText(Core.PathIn(root,".launcher/rollback/files/Support/repair.txt"))=="damaged installed content","Changed target has no recovery copy");
        Need(progress.Messages.Any(m=>m.StartsWith("Comparing installed files")),"Reuse comparison has no progress");
#if MACOS || LINUX
        Need((File.GetUnixFileMode(script)&UnixFileMode.UserExecute)!=0,"Reused launch script lost executable permissions");
#endif
        Core.Rollback(root);
        Need(Core.InstalledVersion(root)=="v1.0.0" && File.ReadAllText(Core.PathIn(root,LauncherPlatform.GameBinary))=="game v1.0.0","Delta rollback did not restore old game/manifest");
        Need(File.GetLastWriteTimeUtc(reused)==stamp && File.ReadAllText(reused)=="unchanged content","Delta rollback changed reused files");
        Need(File.ReadAllText(repair)=="damaged installed content","Delta rollback did not restore the exact previous target bytes");
        Stage(root,"v1.2.0",same);Put(root,".launcher/pending/payload/Support/reused.txt",Bytes("tampered staged file"));
        bool rejected=false;try {Core.InstallPending(root);}catch(InvalidDataException){rejected=true;}
        Need(rejected && Core.InstalledVersion(root)=="v1.0.0","Reuse bypassed staged-file verification");
        Need(File.ReadAllText(reused)=="unchanged content","Rejected update changed reused data");
        Console.WriteLine("PASS shared update reuse: identical files retain timestamps, damaged files repair, permissions recover, delta rollback and staged tampering remain verified");
    }
    public static void Run()
    {
        string root = Path.Combine(AppContext.BaseDirectory, "tsfp restart " + Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        try
        {
            ReuseChecks(root);
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
            var reports = new Reports();
            int result;
            using(var progress = new UpdateProgress(root,reports))
            result = UpdateRestart.ApplyGame(root, Int32.MaxValue, false, () => {
                Need(Core.InstalledVersion(root) == "v1.1.0", "Restart preceded the completed update");
                Need(File.ReadAllText(Core.PathIn(root, Core.LauncherName)) == "new launcher", "Old launcher reopened");
                Need(!Directory.Exists(Core.PathIn(root, ".launcher/pending")), "Restart preceded staging cleanup");
                Need(reports.Messages.Any(m=>m.StartsWith("Cleaning up installation files")),"Restart preceded progress cleanup phase");
                restarted();
            },null,progress);
            foreach(string phase in new[]{"Waiting for the launcher", "Recovering interrupted work", "Verifying staged files", "Backing up saves", "Backing up installed files", "Installing files", "Cleaning up installation files", "Reopening the launcher"})
                Need(reports.Messages.Any(m=>m.StartsWith(phase)),"Missing update progress: "+phase);
            Need(File.ReadAllText(Core.PathIn(root,".launcher/launcher.log")).Contains("Update phase completed:"),"Update timings missing");
            Need(result == 0 && restarts == 1, "Successful update did not restart once");
            Console.WriteLine("PASS shared restart: verified update replaces the launcher before reopening and preserves personal files");

            // Broken feedback must never prevent a restore or change its result.
            reports.Throw=true;
            result = UpdateRestart.ApplyGame(root, Int32.MaxValue, true, restarted,null,reports);
            reports.Throw=false;reports.Messages.Clear();
            Need(result == 0 && restarts == 2 && Core.InstalledVersion(root) == "v1.0.0", "Restore did not reopen the previous game");
            Console.WriteLine("PASS shared restart: restore reopens only after the transaction and lock release");

            Stage(root, "v1.2.0");
            Put(root, ".launcher/pending/payload/" + LauncherPlatform.GameBinary, Bytes("tampered"));
            result = UpdateRestart.ApplyGame(root, Int32.MaxValue, false, restarted,null,reports);
            Need(reports.Messages.Any(m=>m.StartsWith("Update failed")),"Rejected update did not report failure progress");
            Need(!reports.Messages.Any(m=>m.StartsWith("Installing files")),"Tampered update reached installation");
            Need(result == 1 && restarts == 3 && Core.InstalledVersion(root) == "v1.0.0", "Failed update changed the game or failed to reopen");
            Need(File.ReadAllText(Core.PathIn(root, ".launcher/last-result.txt")).StartsWith("Update failed:"), "Failure result missing");
            Need(Directory.Exists(Core.PathIn(root, ".launcher/pending")), "Failure discarded recovery evidence");
            Console.WriteLine("PASS shared restart: rejected payload retains the installed game and reports failure without retrying");

            var start = LauncherPlatform.RestartInfo(root);
            Need(start.WorkingDirectory == root, "Restart changed the installation");
#if LINUX
            Need(start.FileName == Core.PathIn(root,Core.LauncherName) && start.ArgumentList.SequenceEqual(new[] {
                "--skip-self-update", "--root", root }), "Linux restart lost the selected root or retry guard");
#elif MACOS
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
