using System;
using System.IO;
using System.Linq;
using System.Net;
using System.Net.Http;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Threading;
using TimeSplitters.Launcher;

// Real WPF commands, HTTP/validation policy and filesystem operations, isolated
// behind a synthetic HTTP endpoint. No real game or personal files are used.
public static class LauncherAppearanceChecks
{
    sealed class Endpoint:HttpMessageHandler {
        public int Assets;public bool Offline;
        protected override Task<HttpResponseMessage> SendAsync(HttpRequestMessage request,CancellationToken token){
            if(Offline)return Task.FromResult(new HttpResponseMessage(HttpStatusCode.ServiceUnavailable));
            string tag="v9.0.0",name="TimeSplitters-Future-Perfect-PC-"+tag+"-windows-x64.zip";
            if(request.RequestUri.AbsolutePath.EndsWith("/releases"))return Task.FromResult(new HttpResponseMessage(HttpStatusCode.OK){Content=new StringContent(Json.Encode(new[]{new {tag_name=tag,prerelease=false,draft=false,body="Fixture release",assets=new[]{new {name=name,state="uploaded",size=12,digest="sha256:"+new string('a',64),browser_download_url=ReleaseClient.ReleasesPage+"/download/"+tag+"/"+name}}}}))});
            Assets++;return Task.FromResult(new HttpResponseMessage(HttpStatusCode.OK){Content=new ByteArrayContent(Encoding.UTF8.GetBytes("bad-package!"))});
        }
    }
    static void Need(bool condition,string message){if(!condition)throw new Exception("Working appearance: "+message);}
    static System.Collections.Generic.IEnumerable<DependencyObject> Tree(DependencyObject root){yield return root;foreach(var c in LogicalTreeHelper.GetChildren(root).OfType<DependencyObject>().ToArray())foreach(var item in Tree(c))yield return item;}
    static T Named<T>(DependencyObject root,string name) where T:FrameworkElement {return Tree(root).OfType<T>().First(c=>c.Name==name);}
    static void Click(Button button){button.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));}
    static void Until(Func<bool> complete){var frame=new DispatcherFrame();var clock=System.Diagnostics.Stopwatch.StartNew();var timer=new DispatcherTimer {Interval=TimeSpan.FromMilliseconds(20)};timer.Tick+=(s,e)=>{if(complete() || clock.ElapsedMilliseconds>15000){timer.Stop();frame.Continue=false;}};timer.Start();Dispatcher.PushFrame(frame);Need(complete(),"operation did not complete");}
    static void Await(Task task){Until(()=>task.IsCompleted);task.GetAwaiter().GetResult();}
    static void Put(string root,string path,string text){string file=Core.PathIn(root,path);Directory.CreateDirectory(Path.GetDirectoryName(file));File.WriteAllText(file,text);}
    public static int Run(){Exception failure=null;var thread=new Thread(()=>{try{CoreRun();}catch(Exception ex){failure=ex;}});thread.SetApartmentState(ApartmentState.STA);thread.IsBackground=true;thread.Start();Need(thread.Join(90000),"audit timed out");if(failure!=null)throw failure;return 0;}
    static void Scaling(LauncherWindow ui,string root){
        string output=Environment.GetEnvironmentVariable("TSFP_APPEARANCE_RENDERS");if(!String.IsNullOrEmpty(output))Directory.CreateDirectory(output);
        var window=ui.Window;window.WindowStartupLocation=WindowStartupLocation.Manual;window.Left=-20000;window.Top=-20000;window.ShowActivated=false;window.Show();Until(()=>!ui.AppearanceBusy&&!ui.AppearanceChecking);
        foreach(double scale in new[]{1d,1.25,1.5,1.75,2d}){
            var client=new Size(Math.Min(1184,1920/scale),Math.Min(684,1080/scale));var size=LauncherAppearanceDetails.LayoutSize(client);Need(size.Width>=939.9&&size.Height>=639.9,"scaled desktop clips the minimum layout");
            window.MinWidth=0;window.MinHeight=0;window.Width=client.Width;window.Height=client.Height;
            foreach(string page in new[]{"Home","Installation","Game settings","Saves","Repair","Diagnostics","Console"}){
                if(page=="Installation")ui.Appearance.MainAction.Focus();
                ui.Appearance.Navigate(page);Until(()=>!ui.AppearanceBusy&&!ui.AppearanceChecking);Need(ui.AppearancePage==(page=="Installation"?"Home":page),"scaling test failed to open "+page);var visual=(FrameworkElement)window.Content;window.UpdateLayout();
                Need(visual.ActualWidth>0&&visual.ActualHeight>0,"empty viewport at "+scale);
                foreach(var buttons in Tree(visual).OfType<System.Windows.Controls.Primitives.UniformGrid>().Where(g=>g.Children.OfType<Button>().Count()>=6&&g.IsVisible)){
                    var widths=buttons.Children.OfType<Button>().Select(b=>b.ActualWidth).ToArray();Need(widths.Min()>40&&widths.Max()-widths.Min()<1,"tab width overflow at "+scale);
                }
                if(!String.IsNullOrEmpty(output)){var bitmap=new System.Windows.Media.Imaging.RenderTargetBitmap((int)Math.Ceiling(client.Width*scale),(int)Math.Ceiling(client.Height*scale),96*scale,96*scale,System.Windows.Media.PixelFormats.Pbgra32);bitmap.Render(visual);var encoder=new System.Windows.Media.Imaging.PngBitmapEncoder();encoder.Frames.Add(System.Windows.Media.Imaging.BitmapFrame.Create(bitmap));using(var file=File.Create(Path.Combine(output,page.Replace(" ","-")+"-"+scale.ToString(System.Globalization.CultureInfo.InvariantCulture)+".png")))encoder.Save(file);}
            }
        }
        window.Hide();ui.AppearanceNavigate("Home");Console.WriteLine("PASS working layout: 100/125/150/175/200 percent density and desktop-size matrix; tools/settings tabs fit. Physical monitor DPI is unchanged.");
    }
    static void CoreRun(){
        SynchronizationContext.SetSynchronizationContext(new DispatcherSynchronizationContext(Dispatcher.CurrentDispatcher));
        string root=Path.Combine(Path.GetTempPath(),"tsfp-working-appearance-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        var endpoint=new Endpoint();var client=new ReleaseClient(endpoint);var ui=new LauncherWindow(root,false,client);new LauncherAppearance(ui);
        try{
            var focusButton=new Button {Content="Menu option",Style=(Style)ui.Window.FindResource(typeof(Button))};
            var focusWindow=new Window {Content=focusButton,Width=240,Height=100,Left=-20000,Top=-20000,WindowStartupLocation=WindowStartupLocation.Manual};
            try{focusWindow.Show();focusWindow.Activate();Need(focusButton.Focus(),"menu button lost focusability");focusWindow.UpdateLayout();
                var face=(Border)focusButton.Template.FindName("Face",focusButton);
                Need(focusButton.IsKeyboardFocused&&Equals(face.BorderBrush,focusButton.BorderBrush),"focused menu button replaced its normal border");
                Need(focusButton.FocusVisualStyle!=null,"keyboard navigation lost its focus cue");
            }finally{focusWindow.Close();}
            Console.WriteLine("PASS working layout: pointer/programmatic button focus keeps its normal border; keyboard focus adorner retained");
            var shell=ui.Appearance;Need(!shell.MainAction.IsEnabled,"empty installation offered an enabled launch");
            Await(ui.AppearanceCheck());shell.RefreshState();Need(ui.AppearanceCatalog.Length==1 && shell.MainAction.IsEnabled && ui.AppearanceCommand=="UpdateButton","live catalog did not drive Install");
            Put(root,"Saves/profile","my save");Put(root,"custom.txt","keep");
            Click(shell.MainAction);Until(()=>!ui.AppearanceBusy);Need(endpoint.Assets>0 && ui.AppearanceCommand=="RetryButton","primary did not use actual download/recovery command");
            Need(!File.Exists(Core.PathIn(root,"BUILD.json")) && File.ReadAllText(Core.PathIn(root,"Saves/profile"))=="my save" && File.ReadAllText(Core.PathIn(root,"custom.txt"))=="keep","corrupt download changed installed/personal files");
            var stage=new Grid();var host=new Grid();var label=new TextBlock();host.Children.Add(label);stage.Children.Add(host);Release confirmed=null;
            var picker=new GameChannelPickerPage();picker.SetReleases(ui.AppearanceCatalog,"stable");var menu=new GameChannelMenu(ui.Window,stage,label,picker,()=>{},r=>confirmed=r,ui.AppearanceInstalled);
            menu.Update(ui.AppearanceCatalog,ui.AppearanceInstalled,false,false);menu.Select("stable");menu.Confirm();Need(confirmed!=null && confirmed.Tag=="v9.0.0","first-install Stable choice was incorrectly treated as installed");menu.Close();
            Console.WriteLine("PASS working layout: real catalog -> Install -> checksum rejection -> Retry; personal files kept");
            endpoint.Offline=true;Await(ui.AppearanceCheck());Need(ui.AppearanceCommand=="RetryButton","offline failure did not retain real recovery");endpoint.Offline=false;
            ui.AppearanceNavigate("Game settings");var settings=ui.AppearanceSettings;Need(settings!=null,"settings were not reachable");
            var vsync=Named<CheckBox>(settings.View,"VSync");bool initial=vsync.IsChecked==true;vsync.IsChecked=!initial;
            var back=Tree(settings.View).OfType<Button>().First(b=>System.Windows.Automation.AutomationProperties.GetName(b)=="Back to Launcher tools");Click(back);ui.AppearanceNavigate("Game settings");Need(ui.AppearanceSettings==settings && vsync.IsChecked==!initial,"Back lost the native settings draft");
            Click(Named<Button>(settings.View,"SettingsSaveButton"));Need(GameSettings.Load(root,null).VSync==!initial,"real settings did not persist");
            Console.WriteLine("PASS working layout: native settings draft navigation and actual disk persistence");
            ui.AppearanceNavigate("Saves");Click(Named<Button>(ui.Window.Content as DependencyObject,"CreateSaveBackup"));Until(()=>!ui.AppearanceBusy);
            var choices=Named<ComboBox>(ui.Window.Content as DependencyObject,"SaveBackupChoices");Need(choices.Items.Count==1 && choices.IsEnabled,"real backup did not enable restore");
            Put(root,"Saves/profile","new save");choices.SelectedIndex=0;Click(Named<Button>(ui.Window.Content as DependencyObject,"RestoreSaveBackup"));Click(Named<Button>(ui.Window.Content as DependencyObject,"ConfirmSaveRestore"));Until(()=>!ui.AppearanceBusy);
            Need(File.ReadAllText(Core.PathIn(root,"Saves/profile"))=="my save" && SaveBackups.List(root,null).Length==2,"real restore did not preserve a before-restore backup");
            Console.WriteLine("PASS working layout: actual Backup -> confirmation -> Restore with pre-restore preservation");
            ui.AppearanceNavigate("Repair");Click(Named<Button>(ui.Window.Content as DependencyObject,"VerifyGameFiles"));Until(()=>!ui.AppearanceBusy);
            Need(Named<TextBox>(ui.Window.Content as DependencyObject,"VerificationReport").Text.Length>0,"native verification report was lost");
            Console.WriteLine("PASS working layout: real verification remains reachable and reports missing installation");
            ui.AppearanceNavigate("Console");Need(Named<TextBox>(ui.Window.Content as DependencyObject,"GameConsoleOutput").Text.Length==0,"empty Console was fabricated");
            // Reuse the existing inert direct-process probe, never a game binary.
            Directory.CreateDirectory(Core.PathIn(root,"Bin"));File.Copy(typeof(LauncherAppearanceChecks).Assembly.Location,Core.PathIn(root,"Bin/futureperfect_recomp.exe"));Put(root,"Bin/probe-exit.txt","0");Put(root,"Launch.ps1","fixture");
            Await(ui.LaunchGame());Need(Directory.GetFiles(Core.PathIn(root,"Logs"),"*.out").Any(file=>File.ReadAllText(file).Contains("DIRECT_GAME_STARTED")),"real native launch adapter did not run the inert probe");
            Need(Json.String(Json.Object(File.ReadAllText(Core.PathIn(root,".launcher/game-session.json"))),"state")=="clean","probe session did not finish cleanly");
            Console.WriteLine("PASS working layout: actual native process/session/log lifecycle using inert probe (not gameplay)");
            ui.AppearanceNavigate("Home");shell.RefreshState();Need(!((Button)ui.Window.FindName("PlayButton")).IsEnabled && !String.Equals(System.Windows.Automation.AutomationProperties.GetName(shell.MainAction),"Play",StringComparison.OrdinalIgnoreCase),"missing game data bypassed native launch eligibility");
            Console.WriteLine("PASS working layout: missing original game data continues to block Play");
            Scaling(ui,root);
        }finally{ui.Window.Close();var full=Path.GetFullPath(root);if(full.StartsWith(Path.GetFullPath(Path.GetTempPath()),StringComparison.OrdinalIgnoreCase) && Path.GetFileName(full).StartsWith("tsfp-working-appearance-",StringComparison.Ordinal))Directory.Delete(full,true);}
    }
}
