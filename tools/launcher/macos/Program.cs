using System;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Avalonia.Themes.Fluent;
using Avalonia.Styling;
#if LINUX
[assembly: System.Runtime.Versioning.SupportedOSPlatform("linux")]
#endif
#if MACOS
[assembly: System.Runtime.Versioning.SupportedOSPlatform("macos12.0")]
#endif
namespace TimeSplitters.Launcher
{
    public static class Program
    {
        public static string Root;
        public static string[] Arguments;
#if MACOS
        internal static string InstallRoot, StorageError;
#endif
        [STAThread] public static int Main(string[] args)
        {
            Arguments=args;
#if MACOS
            if(args.Length==1 && args[0]=="--test-storage")return MacStorageTests.Run();
#endif
            if(args.Length==3 && args[0]=="--test-self-helper")return NativeStartup.Apply(Path.GetFullPath(args[1]),Int32.Parse(args[2]),true);
            if(args.Length==3 && args[0]=="--apply-self")return NativeStartup.Apply(Path.GetFullPath(args[1]),Int32.Parse(args[2]));
            if(args.Length==3 && (args[0]=="--apply-game" || args[0]=="--rollback-game" || args[0]=="--test-game-helper"))
                return NativeStartup.ApplyGame(Path.GetFullPath(args[1]),Int32.Parse(args[2]),args[0]=="--rollback-game",args[0]=="--test-game-helper");
            if(args.Length==3 && args[0]=="--extract-art") { ExtractArt(args[1],args[2]);return 0; }
            if(args.Length==1 && args[0]=="--test")
#if LINUX
                return LinuxTests.Run();
#else
                return NativeTests.Run();
#endif
#if LINUX
            if(args.Length==3 && args[0]=="--verify-package")return LinuxTests.VerifyPackage(args[1],args[2]);
#endif
#if LINUX
            if(args.Length==3 && args[0]=="--verify-game-package")return NativeGamePackageChecks.Verify(args[1],args[2]);
#endif
#if MACOS
            if(args.Length==3 && args[0]=="--verify-package")return NativeTests.VerifyPackage(args[1],args[2]);
            if(args.Length==3 && args[0]=="--verify-game-package")return NativeTests.VerifyGamePackage(args[1],args[2]);
#endif
            if(args.Length==1 && args[0]=="--test-ui"){AppBuilder.Configure<NativeApplication>().UsePlatformDetect().SetupWithoutStarting();try{
#if MACOS
NativeTests.LocationUi();
#endif
return LauncherSharedUiChecks.Run();}catch(Exception e){Console.Error.WriteLine(e);return 1;}}
#if MACOS
            InstallRoot=FindInstallRoot(args);
            try { Root=FindRoot(args); }
            catch(Exception ex) { StorageError=ex.Message;Root=MacStorage.DefaultRoot; }
#else
            Root=FindRoot(args);
#endif
            if(args.Contains("--check")){try{
#if MACOS
Core.Require(StorageError==null,StorageError);
#endif
Core.Require(!NativeInstallation.Translocated(Root),NativeInstallation.Guidance);NativeHost.CheckData(Root);NativeHost.CheckBuild(Root);GameSettings.Load(Root,null);Console.WriteLine(LauncherPlatform.HostName+" launcher prerequisites passed.");return 0;}catch(Exception e){Console.Error.WriteLine(e.Message);return 1;}}
            return AppBuilder.Configure<NativeApplication>().UsePlatformDetect().StartWithClassicDesktopLifetime(args);
        }
        internal static string FindRoot(string[] args, string baseDirectory=null)
        {
            string source=FindInstallRoot(args,baseDirectory);
#if MACOS
            return MacStorage.Resolve(args,source);
#else
            return source;
#endif
        }
        internal static string FindInstallRoot(string[] args, string baseDirectory=null)
        {
            int option=Array.IndexOf(args,"--root");if(option>=0){if(option+1>=args.Length)throw new ArgumentException("--root needs a path");return Path.GetFullPath(args[option+1]);}
            baseDirectory=baseDirectory??AppContext.BaseDirectory;
            for(var dir=new DirectoryInfo(baseDirectory);dir!=null;dir=dir.Parent)
                if(LauncherPlatform.Checkout(dir.FullName))return dir.FullName;
            var bundle=new DirectoryInfo(baseDirectory).Parent?.Parent;
            if(bundle!=null && bundle.Name.EndsWith(".app"))return bundle.Parent.FullName;
            return baseDirectory;
        }
        public static void Log(string root,string text){LauncherLog.Write(root,text);}
        static void ExtractArt(string path,string destination)
        {
            Directory.CreateDirectory(destination);
            using(var pe=new PEReader(File.OpenRead(path))){var md=pe.GetMetadataReader();var resources=pe.GetSectionData(pe.PEHeaders.CorHeader.ResourcesDirectory.RelativeVirtualAddress);
                foreach(var handle in md.ManifestResources){var resource=md.GetManifestResource(handle);var name=md.GetString(resource.Name);
                    if(!new[]{"launcher-banner.png","launcher-backdrop.png","launcher-logo.png","launcher.ico"}.Contains(name))continue;
                    Core.Require(resource.Implementation.IsNil,"External art resource not supported.");var reader=resources.GetReader((int)resource.Offset,resources.Length-(int)resource.Offset);int size=reader.ReadInt32();Core.Require(size>0 && size<=8*1024*1024,"Invalid art size.");File.WriteAllBytes(Path.Combine(destination,name),reader.ReadBytes(size));Console.WriteLine(name+": "+size+" bytes");}}
        }
    }
    public sealed class NativeApplication:Application
    {
        public override void Initialize(){Name="TimeSplitters Launcher";RequestedThemeVariant=ThemeVariant.Dark;Styles.Add(new FluentTheme());}
        internal static void ApplyWindowIcon(Avalonia.Controls.Window window)
        {
#if LINUX
            // Embed the icon so startup, update/restart and moved installations
            // do not depend on loose artwork files or the working directory.
            using(var stream=typeof(NativeApplication).Assembly.GetManifestResourceStream("launcher.ico"))
                if(stream!=null)window.Icon=new Avalonia.Controls.WindowIcon(stream);
#endif
        }
        internal static Size LauncherClientArea(Size workingArea)
        {
            // Leave room for desktop panels, borders and the window title bar.
            return new Size(Math.Max(1,Math.Min(1152,workingArea.Width-32)),
                            Math.Max(1,Math.Min(772,workingArea.Height-64)));
        }
        internal static Size LauncherLayoutArea(Size client)
        {
            double scale=Math.Min(1,Math.Min(client.Width/1000,client.Height/720));
            return scale>0?new Size(client.Width/scale,client.Height/scale):new Size(1000,720);
        }
        internal static void FitLauncher(Avalonia.Controls.Window window)
        {
            var body=(Avalonia.Controls.Control)window.Content;
            var viewport=new Avalonia.Controls.Viewbox {Stretch=Avalonia.Media.Stretch.Uniform,
                StretchDirection=Avalonia.Media.StretchDirection.DownOnly};
            window.Content=null;viewport.Child=body;window.Content=viewport;
            Action resize=()=>{
                var size=LauncherLayoutArea(window.ClientSize);
                body.Width=size.Width;body.Height=size.Height;
            };
            Action fit=()=>{
                var screen=window.Screens.ScreenFromWindow(window)??window.Screens.Primary;
                if(screen==null)return;
                var area=LauncherClientArea(new Size(screen.WorkingArea.Width/screen.Scaling,
                                                     screen.WorkingArea.Height/screen.Scaling));
                window.MinWidth=Math.Min(640,area.Width);window.MinHeight=Math.Min(400,area.Height);
                window.Width=area.Width;window.Height=area.Height;resize();
            };
            window.PropertyChanged+=(sender,args)=>{
                if(args.Property==Avalonia.Controls.TopLevel.ClientSizeProperty)resize();
            };
            fit();window.Opened+=(sender,args)=>fit();
        }
        internal static Avalonia.Controls.Window StartupWindow(string root,string[] args)
        {
            if(NativeInstallation.Translocated(root)||args.Contains("--render-location-preview"))return new NativeLocationWindow(args);
#if MACOS
            if(Program.InstallRoot!=null && !LauncherPlatform.Checkout(Program.InstallRoot) &&
               (args.Contains("--choose-storage") || Program.StorageError!=null ||
                (!MacStorage.Portable(args,Program.InstallRoot) && !MacStorage.Same(root,Program.InstallRoot))))
                return new NativeStorageWindow(Program.InstallRoot,root,args,Program.StorageError);
#endif
            return args.Any(a=>a.StartsWith("--render-")||a=="--test-session")?(Avalonia.Controls.Window)new NativeWindow(root,args):new NativeStartup(root,args);
        }
        public override void OnFrameworkInitializationCompleted(){if(ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)desktop.MainWindow=StartupWindow(Program.Root,Program.Arguments);base.OnFrameworkInitializationCompleted();}
    }
}
