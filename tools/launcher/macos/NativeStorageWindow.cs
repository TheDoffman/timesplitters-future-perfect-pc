#if MACOS
using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Media;
using Avalonia.Platform.Storage;

namespace TimeSplitters.Launcher
{
    public sealed class NativeStorageWindow : Window
    {
        readonly string source, selected;
        readonly string[] args;
        readonly TextBlock status;
        readonly StackPanel actions;
        readonly Button cancelMigration;
        CancellationTokenSource migrationStop;
        bool working;
        public NativeStorageWindow(string source, string selected, string[] args, string error = null)
        {
            this.source=source;this.selected=selected;this.args=args;
            Title="TimeSplitters · Game storage";Width=620;MaxHeight=720;SizeToContent=SizeToContent.Height;CanResize=false;
            WindowStartupLocation=WindowStartupLocation.CenterScreen;Background=Brush.Parse("#0B1420");
            var panel=new StackPanel { Margin=new Thickness(28),Spacing=14 };
            panel.Children.Add(NativeWindow.Text("CHOOSE YOUR GAME FOLDER",22));
            panel.Children.Add(NativeWindow.Text("Keep your game, saves, settings, textures and logs separate from the app. You can then move or replace the app without moving your progress.",15));
            panel.Children.Add(NativeWindow.Text("Recommended: "+MacStorage.DefaultRoot));
            if(!MacStorage.Same(selected,MacStorage.DefaultRoot))panel.Children.Add(NativeWindow.Text("Current: "+selected));
            panel.Children.Add(NativeWindow.Text("Existing installations beside the app are copied to Application Support automatically. For files elsewhere, choose their folder to use it in place, or copy it to Application Support. Copies are verified before switching; the original folder is kept as a backup."));
            status=NativeWindow.Text(error??"");panel.Children.Add(status);
            actions=new StackPanel { Spacing=10 };panel.Children.Add(actions);
            actions.Children.Add(NativeWindow.Button("Use Application Support",()=>_ = Use(MacStorage.DefaultRoot,true)));
            actions.Children.Add(NativeWindow.Button("Choose existing game folder…",()=>_ = Choose(false)));
            actions.Children.Add(NativeWindow.Button("Copy existing folder to Application Support…",()=>_ = Choose(true)));
            if(MacStorage.HasInstallation(source))
                actions.Children.Add(NativeWindow.Button("Use existing files beside this app",()=>_ = Use(source,true)));
            actions.Children.Add(NativeWindow.Button("Keep portable (beside this app)",()=>_ = Use(source,false,true)));
            actions.Children.Add(NativeWindow.Button("Quit",Close));
            cancelMigration=NativeWindow.Button("Cancel migration",()=>migrationStop?.Cancel());
            cancelMigration.IsVisible=false;panel.Children.Add(cancelMigration);
            Content=new Border { Background=Background,Child=NativeWindow.Scroll(panel) };
            Opened+=(s,e)=>{
                var screen=Screens.ScreenFromWindow(this)??Screens.Primary;
                if(screen!=null){MaxHeight=Math.Max(1,screen.WorkingArea.Height/screen.Scaling-64);Width=Math.Min(620,Math.Max(1,screen.WorkingArea.Width/screen.Scaling-32));}
            };
            Closing+=(s,e)=>{if(working)e.Cancel=true;};
            // Subsequent Finder launches forward to the already chosen writable
            // installation; a missing drive never silently creates a fresh one.
            if(error==null && !args.Contains("--choose-storage")) {
                try {
                    string legacy=MacStorageMigration.AutomaticSource(args,source,selected);
                    if(legacy!=null)Opened+=async(s,e)=>await Use(selected,true,false,legacy);
                    else if(File.Exists(Core.PathIn(selected,Core.SelfManifest)))Opened+=async(s,e)=>await Use(selected,false);
                } catch(Exception ex) { status.Text=ex.Message; }
            }
        }
        async Task Choose(bool import)
        {
            try {
                var picked=await StorageProvider.OpenFolderPickerAsync(new FolderPickerOpenOptions {
                    Title="Choose the folder containing Data, Saves and Settings",AllowMultiple=false });
                string folder=picked.FirstOrDefault()?.TryGetLocalPath();
                if(folder!=null) {
                    folder=MacStorage.ExistingFolder(folder);
                    if(!import || MacStorage.Same(folder,MacStorage.DefaultRoot))await Use(folder,true);
                    else await Use(MacStorage.DefaultRoot,true,false,folder);
                }
            } catch(Exception ex) { status.Text=ex.Message; }
        }
        async Task Use(string target,bool remember,bool portable=false,string legacy=null)
        {
            if(working)return;
            working=true;actions.IsEnabled=false;status.Text="Preparing your game folder…";
            try {
                if(legacy==null && remember && MacStorage.Same(target,MacStorage.DefaultRoot) && !MacStorage.HasInstallation(target))
                    legacy=MacStorageMigration.PendingSource(target)??(!MacStorage.Same(source,target) && MacStorage.HasInstallation(source) &&
                        !MacStorage.HasPersonalFiles(target) && !MacStorageMigration.CompletedFrom(target,source)?source:null);
                if(legacy!=null) {
                    var progress=new Progress<string>(message=>status.Text=message);
                    using(var transfer=new CancellationTokenSource()) {
                        migrationStop=transfer;cancelMigration.IsVisible=true;
                        try { await Task.Run(()=>MacStorageMigration.Migrate(legacy,target,source,progress,transfer.Token)); }
                        finally { migrationStop=null;cancelMigration.IsVisible=false; }
                    }
                } else await Task.Run(()=>MacStorage.Prepare(source,target));
                if(remember)MacStorage.Remember(target);
                if(portable)Core.Write(Core.PathIn(target,".tsfp-portable"),"Portable TimeSplitters installation\n");
                else if(remember && File.Exists(Core.PathIn(source,".tsfp-portable")))
                    File.Delete(Core.PathIn(source,".tsfp-portable"));
                using(var process=System.Diagnostics.Process.Start(MacStorage.StartInfo(target,args))) {
                    Core.Require(process!=null,"The launcher could not be opened. Your files were kept.");
                    await process.WaitForExitAsync();
                    Core.Require(process.ExitCode==0,"macOS could not open the launcher in your game folder. Your files were kept. Retry, or choose another folder.");
                }
                working=false;
                ((IClassicDesktopStyleApplicationLifetime)Application.Current.ApplicationLifetime).Shutdown();
            } catch(OperationCanceledException) { status.Text="Migration canceled. Your original files were kept. Reopen the app to resume, or keep using the existing folder."; }
            catch(Exception ex) { status.Text=ex.Message; }
            finally { migrationStop=null;cancelMigration.IsVisible=false;working=false;actions.IsEnabled=true; }
        }
    }
}
#endif
