using System;
using System.Collections.Generic;
using System.IO;
namespace TimeSplitters.Launcher
{
    // Exercise the actual installer and native build validation on both hosts.
    public static class NativeGamePackageChecks
    {
        public static int Verify(string archive,string version,Func<string,bool> hostCheck=null)
        {
            string test=Path.Combine(Path.GetTempPath(),"tsfp-native-game-package-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(test);
            try {
                foreach(bool preserve in new[]{false,true}){
                    string root=Path.Combine(test,preserve?"existing":"fresh"),pending=Path.Combine(root,".launcher/pending");Directory.CreateDirectory(pending);
                    void Write(string name,string value){string path=Core.PathIn(root,name);Directory.CreateDirectory(Path.GetDirectoryName(path));File.WriteAllText(path,value);}
                    if(preserve){Write("Data/keep","game data");Write("Saves/keep","save");Write("Settings/pc-controls.json","personal controls");Write("Settings/pc-display.ini","personal display");}
                    Core.Unpack(archive,Path.Combine(pending,"payload"),version);Core.Write(Path.Combine(pending,"release.json"),Json.Encode(new{version=version}));Core.InstallPending(root);
                    var manifest=NativeHost.CheckBuild(root);var files=(Dictionary<string,object>)manifest["files"];
                    foreach(var pair in files){if(preserve&&pair.Key=="Settings/pc-controls.json")continue;Core.Require(Core.Hash(Core.PathIn(root,pair.Key))==(string)pair.Value,"Installed file changed: "+pair.Key);}
                    Core.Require(SelfUpdate.Complete(root)&&(hostCheck==null||hostCheck(root)),"Bundled launcher failed host validation");
                    string exe=Core.PathIn(root,LauncherPlatform.GameBinary);byte original;
                    using(var stream=new FileStream(exe,FileMode.Open,FileAccess.ReadWrite)){original=(byte)stream.ReadByte();stream.Position=0;stream.WriteByte((byte)(original^1));}
                    bool rejected=false;try{NativeHost.CheckBuild(root);}catch(InvalidDataException){rejected=true;}
                    Core.Require(rejected,"Corrupted game was accepted");
                    using(var stream=new FileStream(exe,FileMode.Open,FileAccess.Write)){stream.WriteByte(original);}NativeHost.CheckBuild(root);
                    if(preserve)Core.Require(File.ReadAllText(Core.PathIn(root,"Data/keep"))=="game data"&&File.ReadAllText(Core.PathIn(root,"Saves/keep"))=="save"&&File.ReadAllText(Core.PathIn(root,"Settings/pc-controls.json"))=="personal controls"&&File.ReadAllText(Core.PathIn(root,"Settings/pc-display.ini"))=="personal display","Personal files changed");
                    Console.WriteLine(LauncherPlatform.HostName+" game package installed/verified "+files.Count+" files; corruption rejected; personal preservation="+preserve);
                }return 0;
            }catch(Exception error){Console.Error.WriteLine(error);return 1;}
            finally{Directory.Delete(test,true);}
        }
    }
}
