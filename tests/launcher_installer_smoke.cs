// Tiny process/version/update fixtures. Never execute a real game or touch personal files.
using System;
using System.Diagnostics;
using System.IO;
using System.Threading;
using TimeSplitters.Launcher;
public class InstallerSmoke
{
    public static int Main(string[] args)
    {
        string root=Path.GetFullPath(args[1]);
        if(args[0]=="--game") {
            string ready=Path.Combine(root,"fake-game-ready"),stop=Path.Combine(root,"fake-game-stop");File.WriteAllText(ready,"ready");
            var timer=Stopwatch.StartNew();while(!File.Exists(stop) && timer.Elapsed.TotalSeconds<20)Thread.Sleep(50);return 0;
        }
        if(args[0]=="--update") {
            using(var mutex=new Mutex(false,Core.MutexName(root))) {
                if(!mutex.WaitOne(0))throw new Exception("Fixture installation busy");
                try {
                    string payload=Path.Combine(root,".launcher/installer-update-fixture");
                    var changes=SelfUpdate.Unpack(args[2],payload,SelfUpdate.InstalledVersion(root));Core.ApplyLauncher(root,changes,null);
                    if(!SelfUpdate.Complete(root))throw new Exception("Updated files invalid");
                    File.AppendAllText(Path.Combine(root,Core.LauncherName),"updated bytes");
                    var json=Json.Object(File.ReadAllText(Path.Combine(root,Core.SelfManifest)));
                    ((System.Collections.Generic.Dictionary<string,object>)json["files"])[Core.LauncherName]=Core.Hash(Path.Combine(root,Core.LauncherName));
                    File.WriteAllText(Path.Combine(root,Core.SelfManifest),Json.Encode(json));
                    SelfUpdate.CleanupAfterStartup(root);
                }finally{mutex.ReleaseMutex();}
            }
            return 0;
        }
        throw new ArgumentException("Unknown fixture action");
    }
}
