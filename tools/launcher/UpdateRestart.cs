using System;
using System.IO;
using System.Diagnostics;

namespace TimeSplitters.Launcher
{
    // Synchronous producer; UI adapters marshal notifications onto their dispatcher.
    // Observers cannot interrupt a transaction or defeat recovery.
    public sealed class UpdateProgress : IProgress<string>, IDisposable
    {
        readonly string root;
        readonly IProgress<string> target;
        readonly Stopwatch elapsed = Stopwatch.StartNew();
        string phase;
        public UpdateProgress(string root, IProgress<string> target = null) { this.root = root; this.target = target; }
        void FinishPhase() {
            if (phase != null) LauncherLog.Write(root, "Update phase completed: " + phase + " (" + elapsed.Elapsed.TotalSeconds.ToString("F2", System.Globalization.CultureInfo.InvariantCulture) + " s)");
        }
        public void Report(string message) {
            string next = message.Split(new[] { " · " }, StringSplitOptions.None)[0];
            if (next != phase) { FinishPhase(); phase = next; elapsed.Restart(); LauncherLog.Write(root, "Update phase: " + phase); }
            Send(target, message);
        }
        public void Dispose() { FinishPhase(); phase = null; }
        public static void Send(IProgress<string> progress, string message) {
            if (progress != null) try { progress.Report(message); } catch { /* Feedback must not change update/recovery results. */ }
        }
        public static void Count(IProgress<string> progress, string phase, long completed, long total) {
            Send(progress, phase + " · " + (total == 0 ? 100 : completed * 100 / total) + "%");
        }
    }

    // Both hosts finish game updates outside the running UI, then reopen the
    // installed launcher. Only process startup and installation locks vary.
    public static class UpdateRestart
    {
        public static int ApplyGame(string root, int parent, bool rollback,
            Action restart, Action<Exception> reportFailure = null, IProgress<string> progress = null)
        {
            UpdateProgress.Send(progress, "Waiting for the launcher to close");
            SelfUpdate.WaitForParent(parent);
            Exception failure = null;
            using (LauncherPlatform.AcquireUpdateLock(root))
            {
                try
                {
                    Core.Require(!Core.GameRunning(), "Close the game before changing the installed release.");
                    UpdateProgress.Send(progress, "Recovering interrupted work");
                    Core.Recover(root);
                    if (rollback) Core.Rollback(root, progress); else Core.InstallPending(root, progress);
                    Core.Write(Core.PathIn(root, ".launcher/last-result.txt"), rollback
                        ? "Previous release restored." : "Release files installed and verified successfully.");
                }
                catch (Exception ex)
                {
                    failure = ex;
                    UpdateProgress.Send(progress, "Update failed; reopening the recovered launcher");
                    LauncherLog.Write(root, ex.ToString());
                    string detail = AntivirusErrors.Find(ex) == null ? ex.Message : RecoveryInfo.From(ex, false).Message;
                    Core.Write(Core.PathIn(root, ".launcher/last-result.txt"),
                        "Update failed: " + detail + " Check launcher.log before retrying.");
                }
            }
            if (failure != null && reportFailure != null) reportFailure(failure);
            // Reopen the recovered installation after an unsuccessful update too.
            // The new process skips its network update check once to avoid loops.
            UpdateProgress.Send(progress, "Reopening the launcher");
            if (File.Exists(Core.PathIn(root, Core.LauncherName))) restart();
            return failure == null ? 0 : 1;
        }
    }
}
