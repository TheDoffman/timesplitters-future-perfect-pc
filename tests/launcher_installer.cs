using System;
using System.IO;
using Microsoft.Win32;
using TimeSplitters.Launcher;

public static class LauncherInstallerChecks
{
    static int passed;
    static void Need(bool value) {if(!value)throw new Exception("Installer registration assertion failed.");}
    static void Test(string name,Action body) {body();passed++;Console.WriteLine("PASS "+name);}
    public static int Run()
    {
        string root=Path.Combine(Path.GetTempPath(),"tsfp-installer-registration-"+Guid.NewGuid().ToString("N"));
        string path=@"Software\TSFP-InstallerTests\"+Guid.NewGuid().ToString("N");
        Directory.CreateDirectory(Path.Combine(root,"Support/Launcher"));Directory.CreateDirectory(Path.Combine(root,"Bin"));
        File.WriteAllText(Path.Combine(root,Core.LauncherName),"synthetic bootstrap");File.WriteAllText(Path.Combine(root,Core.AppPath),"synthetic app");
        var files=new System.Collections.Generic.Dictionary<string,string>{{Core.LauncherName,Core.Hash(Path.Combine(root,Core.LauncherName))},{Core.AppPath,Core.Hash(Path.Combine(root,Core.AppPath))}};
        string manifest=Json.Encode(new {schema=1,version="1.2.99",files=files});File.WriteAllText(Path.Combine(root,Core.SelfManifest),manifest);
        try {
            Test("portable startup does not create an installer registration",()=>{InstallerIntegration.RefreshVersion(root,path);Need(Registry.CurrentUser.OpenSubKey(path)==null);});
            using(var key=Registry.CurrentUser.CreateSubKey(path)) {
                key.SetValue("InstallLocation",root+"\\");key.SetValue("DisplayVersion","1.2.12");key.SetValue("UninstallString","retained uninstaller");
                Test("registered install reflects verified self-updated launcher version",()=>{InstallerIntegration.RefreshVersion(root,path);Need((string)key.GetValue("DisplayVersion")=="1.2.99" && (string)key.GetValue("UninstallString")=="retained uninstaller");});
                Test("another installation cannot change this registration",()=>{key.SetValue("DisplayVersion","keep");key.SetValue("InstallLocation",root+"-other");InstallerIntegration.RefreshVersion(root,path);Need((string)key.GetValue("DisplayVersion")=="keep");key.SetValue("InstallLocation",root);});
                Test("corrupt launcher files do not update Installed apps version",()=>{File.WriteAllText(Path.Combine(root,Core.AppPath),"damaged");InstallerIntegration.RefreshVersion(root,path);Need((string)key.GetValue("DisplayVersion")=="keep");File.WriteAllText(Path.Combine(root,Core.AppPath),"synthetic app");});
                Test("missing or malformed install locations are preserved",()=>{foreach(string value in new[]{"","bad\0path"}){key.SetValue("InstallLocation",value);InstallerIntegration.RefreshVersion(root,path);Need((string)key.GetValue("DisplayVersion")=="keep");}});
            }
        }finally {
            Registry.CurrentUser.DeleteSubKeyTree(path,false);
            Need(Path.GetFullPath(root).StartsWith(Path.GetFullPath(Path.GetTempPath()),StringComparison.OrdinalIgnoreCase));Directory.Delete(root,true);
        }
        Console.WriteLine("PASS: "+passed+" isolated installer registration checks.");return 0;
    }
}
