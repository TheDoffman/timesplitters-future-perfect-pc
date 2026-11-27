using System;
using System.Diagnostics;
using System.IO;

namespace TimeSplitters.Launcher
{
    // The download bootstrap hands installation back to the existing launcher
    // verifier, lock and recovery transaction. Game data is never a payload.
    internal static class LinuxInstaller
    {
        internal static bool Install(string root,string archive,string version)
        {
            root=Path.GetFullPath(root);archive=Path.GetFullPath(archive);
            Core.Require(version==LauncherVersion.Value,"The installer and launcher package versions do not match.");
            Core.Require(!LauncherPlatform.Checkout(root),"Install into a separate folder, not a developer checkout.");
            Directory.CreateDirectory(root);
            Directory.CreateDirectory(Core.PathIn(root,".launcher"));
            using(var held=LauncherPlatform.AcquireUpdateLock(root))
            {
                Core.Require(!Core.GameRunning(),"Close the game before installing the launcher.");
                Core.RecoverLauncher(root);
                string staging=".launcher/linux-install-"+Guid.NewGuid().ToString("N");
                try
                {
                    Core.CheckSpace(root,new FileInfo(archive).Length*4+32*1024*1024);
                    var changes=SelfUpdate.Unpack(archive,Core.PathIn(root,staging),version);
                    var application=Json.Object(File.ReadAllText(Core.PathIn(root,staging+"/"+LauncherPlatform.LauncherApplication)));
                    Core.Require(Json.String(application,"platform")==LauncherPlatform.AssetSuffix,"The launcher package belongs to a different Linux architecture.");
                    Core.Require(Json.String(application,"installer_protocol")=="1","This package predates automatic installation. Use its ZIP installation instructions.");
                    using(var stream=File.OpenRead(Core.PathIn(root,staging+"/"+LauncherPlatform.LauncherBinary)))
                    {
                        var header=new byte[20];
                        Core.Require(stream.Read(header,0,header.Length)==header.Length && LauncherPlatform.LinuxElfMatches(header,LauncherPlatform.AssetSuffix),"The launcher executable does not match this Linux architecture.");
                    }
                    string installed=SelfUpdate.InstalledVersion(root);
                    if(installed.Length>0)
                    {
                        int order=GameVersion.Parse(version).CompareTo(GameVersion.Parse(installed));
                        Core.Require(order>=0,"A newer launcher is already installed. Open it instead of downgrading.");
                        if(order==0 && SelfUpdate.Complete(root))
                        {
                            var current=Json.Object(File.ReadAllText(Core.PathIn(root,LauncherPlatform.LauncherApplication)));
                            if(Json.String(current,"platform")==LauncherPlatform.AssetSuffix)return false;
                        }
                    }
                    Core.ApplyLauncher(root,changes,null);
                    Core.Require(SelfUpdate.Complete(root),"Installed launcher verification failed.");
                    return true;
                }
                finally { Core.RemoveOwned(root,staging); }
            }
        }

        internal static int Run(string[] args)
        {
            try
            {
                Core.Require(args.Length==4 || args.Length==5 && args[4]=="--no-launch","Use --install-launcher INSTALL_FOLDER ARCHIVE VERSION [--no-launch].");
                string root=Path.GetFullPath(args[1]);
                bool changed=Install(root,args[2],args[3]);
                Console.WriteLine((changed?"Installed ":"Already installed: ")+LauncherPlatform.HostName+" launcher in "+root);
                if(args.Length==4)Process.Start(LauncherPlatform.RestartInfo(root));
                return 0;
            }
            catch(Exception error) {Console.Error.WriteLine(error.Message);return 1;}
        }
    }
}
