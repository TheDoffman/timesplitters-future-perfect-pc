// Exercise shell failures with injected actions; never start a real application or game.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.IO;
using System.Linq;
using System.Threading;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using TimeSplitters.Launcher;

static class LauncherStartupChecks
{
    static string root;
    static int passed;
    static void Need(bool value) { if(!value)throw new Exception("Startup recovery assertion failed."); }
    static void Test(string name,Action body) { body();passed++;Console.WriteLine("PASS "+name); }
    sealed class Fixture : IDisposable
    {
        public readonly StartupWindow UI;
        public bool Closed;
        public Fixture(Action launch,Action<string> folder=null) {
            UI=new StartupWindow(root,true,true,launch,folder??(path=>{}));
            UI.Window.Closed+=(s,e)=>Closed=true;
        }
        public Button Button(string name) { return (Button)UI.Window.FindName(name); }
        public string Text(string name) { return ((TextBlock)UI.Window.FindName(name)).Text; }
        public void Click(string name) { Button(name).RaiseEvent(new RoutedEventArgs(System.Windows.Controls.Button.ClickEvent)); }
        public void Dispose() { if(!Closed)UI.Window.Close(); }
    }
    static void Layout(StartupWindow ui)
    {
        var grid=(Grid)ui.Window.Content;
        grid.Background=ui.Window.Background;
        grid.Measure(new Size(544,350));grid.Arrange(new Rect(0,0,544,350));grid.UpdateLayout();
        var status=(TextBlock)ui.Window.FindName("Status");
        var progress=(ProgressBar)ui.Window.FindName("Progress");
        Need(status.TranslatePoint(new Point(0,status.ActualHeight),grid).Y<=progress.TranslatePoint(new Point(0,0),grid).Y);
        foreach(string name in new[]{"Retry","Folder","Logs"}) {
            var button=(Button)ui.Window.FindName(name);
            var point=button.TranslatePoint(new Point(0,0),grid);
            Need(point.X>=0 && point.Y>=0 && point.X+button.ActualWidth<=grid.ActualWidth+1 && point.Y+button.ActualHeight<=grid.ActualHeight+1);
            Need(point.Y>=progress.TranslatePoint(new Point(0,progress.ActualHeight),grid).Y);
        }
    }
    public static int Run()
    {
        Exception failure=null;
        var thread=new Thread(()=> {try{RunSTA();}catch(Exception ex){failure=ex;}});
        thread.SetApartmentState(ApartmentState.STA);thread.Start();
        Need(thread.Join(30000));if(failure!=null)throw failure;
        Console.WriteLine("PASS: "+passed+" isolated startup recovery checks.");return 0;
    }
    static void RunSTA()
    {
        string output=Path.GetDirectoryName(typeof(LauncherStartupChecks).Assembly.Location);
        root=Path.Combine(output,"startup-tests-"+Guid.NewGuid().ToString("N"));
        Directory.CreateDirectory(root);
        var original=new Dictionary<string,string>();
        foreach(string name in new[]{Core.AppPath,"Saves/profile","Settings/pc-controls.json","Data/default.xbe","original.iso",".launcher/downloads/game.part"}) {
            string path=Core.PathIn(root,name);Directory.CreateDirectory(Path.GetDirectoryName(path));
            File.WriteAllText(path,"preserved synthetic fixture: "+name);original[name]=Core.Hash(path);
        }
        try {
            Test("Windows cancellation uses its native code and explains possible prompts without blaming the user",()=> {
                var info=RecoveryInfo.LauncherStart(new Win32Exception(1223,"localized message"));
                Need(info.Message.Contains("SmartScreen") && info.RetryLabel=="Retry" && !info.Message.Contains("localized message") && !info.Message.Contains("cancelled by the user"));
                Need(!RecoveryInfo.LauncherStart(new Win32Exception(unchecked((int)0x80004005))).Message.Contains("SmartScreen"));
            });
            Test("access denial and missing files have distinct recovery guidance",()=> {
                foreach(Exception ex in new Exception[]{new Win32Exception(5),new UnauthorizedAccessException()})
                    Need(RecoveryInfo.LauncherStart(ex).Title=="Windows blocked launcher startup");
                foreach(Exception ex in new Exception[]{new Win32Exception(2),new Win32Exception(3),new FileNotFoundException(),new InvalidDataException("Launcher application files are missing.")})
                    Need(RecoveryInfo.LauncherStart(ex).Title=="Launcher files are missing");
            });
            Test("cancelled shell launch keeps startup open with retry, folder and logs",()=> {
                using(var f=new Fixture(()=>{throw new Win32Exception(1223);})) {
                    f.Click("Continue");Need(!f.Closed && f.Text("Status").Contains("SmartScreen"));
                    foreach(string name in new[]{"Retry","Folder","Logs"})Need(f.Button(name).Visibility==Visibility.Visible && f.Button(name).IsEnabled);
                    Need(f.Button("Continue").Visibility==Visibility.Collapsed && !( (ProgressBar)f.UI.Window.FindName("Progress")).IsIndeterminate);
                    Need(File.ReadAllText(Core.PathIn(root,".launcher/launcher.log")).Contains("Windows error 1223"));
                }
            });
            Test("repeated retry remains usable and closes only after a successful launch",()=> {
                int calls=0;using(var f=new Fixture(()=>{if(++calls<3)throw new Win32Exception(1223);})) {
                    f.Click("Continue");Need(calls==1 && !f.Closed);
                    f.Click("Retry");Need(calls==2 && !f.Closed);
                    f.Click("Retry");Need(calls==3 && f.Closed);
                }
            });
            Test("folder and logs actions open the selected installation without retrying the application",()=> {
                int calls=0;var folders=new List<string>();
                using(var f=new Fixture(()=>{calls++;throw new Win32Exception(1223);},path=>folders.Add(path))) {
                    f.Click("Continue");f.Click("Folder");f.Click("Logs");
                    Need(calls==1 && !f.Closed && folders.SequenceEqual(new[]{root,Core.PathIn(root,".launcher")}));
                }
            });
            Test("folder failures preserve startup guidance and do not accumulate messages",()=> {
                using(var f=new Fixture(()=>{throw new Win32Exception(1223);},path=>{throw new Win32Exception(5);})) {
                    f.Click("Continue");f.Click("Folder");string text=f.Text("Status");f.Click("Folder");
                    Need(!f.Closed && text==f.Text("Status") && text.Contains("SmartScreen") && text.Contains("File Explorer"));Layout(f.UI);
                }
            });
            Test("unexpected launch failures remain recoverable without claiming a security block",()=> {
                using(var f=new Fixture(()=>{throw new IOException("Synthetic failure");})) {
                    f.Click("Continue");Need(!f.Closed && f.Button("Retry").Visibility==Visibility.Visible && !f.Text("Status").Contains("SmartScreen"));
                }
            });
            Test("closing a failed startup does not launch another process",()=> {
                int calls=0;using(var f=new Fixture(()=>{calls++;throw new Win32Exception(1223);})) {
                    f.Click("Continue");f.UI.Window.Close();Need(f.Closed && calls==1);
                }
            });
            Test("security recovery text and actions fit the startup window",()=> {
                using(var f=new Fixture(()=>{throw new Win32Exception(1223);})) {
                    f.Click("Continue");Layout(f.UI);
                    var bitmap=new RenderTargetBitmap(544,350,96,96,PixelFormats.Pbgra32);bitmap.Render((Grid)f.UI.Window.Content);
                    var encoder=new PngBitmapEncoder();encoder.Frames.Add(BitmapFrame.Create(bitmap));
                    using(var stream=File.Create(Path.Combine(output,"startup-recovery-preview.png")))encoder.Save(stream);
                }
            });
            Test("startup recovery preserves application, saves, settings, ISO, data and partial downloads",()=> {
                foreach(var item in original)Need(Core.Hash(Core.PathIn(root,item.Key))==item.Value);
            });
        } finally {
            Need(Path.GetDirectoryName(root)==output);Directory.Delete(root,true);
        }
    }
}
