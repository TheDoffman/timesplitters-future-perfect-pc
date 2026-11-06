using System;
using System.IO;

namespace TimeSplitters.Launcher
{
    // Both hosts finish game updates outside the running UI, then reopen the
    // installed launcher. Only process startup and installation locks vary.
    public static class UpdateRestart
    {
        public static int ApplyGame(string root, int parent, bool rollback,
            Action restart, Action<Exception> reportFailure = null)
        {
            SelfUpdate.WaitForParent(parent);
            Exception failure = null;
            using (LauncherPlatform.AcquireUpdateLock(root))
            {
                try
                {
                    Core.Require(!Core.GameRunning(), "Close the game before changing the installed release.");
                    Core.Recover(root);
                    if (rollback) Core.Rollback(root); else Core.InstallPending(root);
                    Core.Write(Core.PathIn(root, ".launcher/last-result.txt"), rollback
                        ? "Previous release restored." : "Release files installed and verified successfully.");
                }
                catch (Exception ex)
                {
                    failure = ex;
                    LauncherLog.Write(root, ex.ToString());
                    string detail = AntivirusErrors.Find(ex) == null ? ex.Message : RecoveryInfo.From(ex, false).Message;
                    Core.Write(Core.PathIn(root, ".launcher/last-result.txt"),
                        "Update failed: " + detail + " Check launcher.log before retrying.");
                }
            }
            if (failure != null && reportFailure != null) reportFailure(failure);
            // Reopen the recovered installation after an unsuccessful update too.
            // The new process skips its network update check once to avoid loops.
            if (File.Exists(Core.PathIn(root, Core.LauncherName))) restart();
            return failure == null ? 0 : 1;
        }
    }
}
