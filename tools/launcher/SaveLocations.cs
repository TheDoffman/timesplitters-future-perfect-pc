using System;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading;

namespace TimeSplitters.Launcher
{
    // The game reads this same UTF-8 path beside its controls file.
    public static class SaveLocations
    {
        public static StringComparison Comparison { get { return Path.DirectorySeparatorChar=='\\'?StringComparison.OrdinalIgnoreCase:StringComparison.Ordinal; } }
        public static string Default(string root) { return Core.PathIn(root,LauncherPlatform.Saves(root)); }
        public static string Preference(string root) { return Core.PathIn(root,Path.GetDirectoryName(LauncherPlatform.Settings(root)[0]).Replace('\\','/')+"/save-location.txt"); }
        public static string Validate(string path)
        {
            Core.Require(!String.IsNullOrWhiteSpace(path) && Encoding.UTF8.GetByteCount(path)<4095 && !path.Any(Char.IsControl) && Path.IsPathRooted(path),"Choose an absolute save folder path.");
            string absolute=Path.GetFullPath(path);
            Core.Require(String.Equals(Path.GetPathRoot(path).Replace('\\','/'),Path.GetPathRoot(absolute).Replace('\\','/'),Comparison),"Choose a fully qualified folder path.");
            string full=absolute.TrimEnd(Path.DirectorySeparatorChar,Path.AltDirectorySeparatorChar);
            Core.Require(full.Length>Path.GetPathRoot(absolute).TrimEnd(Path.DirectorySeparatorChar,Path.AltDirectorySeparatorChar).Length,"Choose a folder, not the root of a drive.");
            // Reuse the launcher's existing linked-path checks.
            Core.PathIn(full,".tsfp-location-check");return full;
        }
        public static string Resolve(string root)
        {
            string file=Preference(root);if(!File.Exists(file))return Default(root);
            Core.Require(new FileInfo(file).Length<4096,"The saved location is invalid. Choose a save folder again.");
            string path=Validate(new UTF8Encoding(false,true).GetString(File.ReadAllBytes(file)).TrimEnd('\r','\n'));
            Core.Require(Directory.Exists(path),"The selected save folder is unavailable. Reconnect its drive or choose another folder. Existing saves were kept.");
            return path;
        }
        public static void Choose(string root,string selected,bool copy,CancellationToken token,bool installationLockHeld=false)
        {
            Directory.CreateDirectory(Core.PathIn(root,".launcher"));
            using(LauncherPlatform.AcquireSaveLock(root,installationLockHeld)) {
                Core.Require(!Core.GameRunning(),"Close the game before changing the save folder.");SaveBackups.Recover(root);
                string target=Validate(selected),source=null;
                try {source=Resolve(root);}catch(Exception ex){if(copy || !(ex is IOException || ex is InvalidDataException))throw;}
                if(source!=null && String.Equals(source,target,Comparison))return;
                Core.Require(!String.Equals(target,Path.GetFullPath(root).TrimEnd(Path.DirectorySeparatorChar),Comparison),"Choose a dedicated save folder, such as the game's Saves folder.");
                Core.Require(source==null || !(target+Path.DirectorySeparatorChar).StartsWith(source.TrimEnd(Path.DirectorySeparatorChar)+Path.DirectorySeparatorChar,Comparison),"Choose a folder outside the current save folder.");
                Directory.CreateDirectory(target);
                if(copy)Core.Require(!Directory.EnumerateFileSystemEntries(target).Any(),"Copy saves into an empty folder. Existing destination files were kept.");
                if(!copy)Core.Require(!Directory.EnumerateFileSystemEntries(target).Any() || Directory.Exists(Core.PathIn(target,"UserData")),"Choose an empty folder or an existing TimeSplitters save folder.");
                string probe=Core.PathIn(target,".tsfp-write-check-"+Guid.NewGuid().ToString("N"));
                using(var file=new FileStream(probe,FileMode.CreateNew,FileAccess.Write,FileShare.None,1,FileOptions.DeleteOnClose)){file.WriteByte(0);file.Flush(true);}
                if(copy && Directory.Exists(source))SaveBackups.CopyLocation(source,target,token);
                token.ThrowIfCancellationRequested();
                // Default is represented by no preference so portable installs can move.
                if(String.Equals(target,Default(root),Comparison)){if(File.Exists(Preference(root)))File.Delete(Preference(root));}
                else Core.Write(Preference(root),target+"\n");
            }
        }
    }
}
