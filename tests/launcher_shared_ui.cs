using System;
using System.IO;
using System.Linq;
using System.Collections.Generic;
using TimeSplitters.Launcher;
#if MACOS
using Avalonia;
using Avalonia.Controls;
using Avalonia.Interactivity;
using Avalonia.LogicalTree;
using Element = Avalonia.Controls.Control;
#else
using System.Windows;
using System.Windows.Controls;
using Element = System.Windows.FrameworkElement;
#endif

// Exactly the same behavioral contracts run against the real WPF and Avalonia pages.
public static class LauncherSharedUiChecks
{
    static int passed;
    static void Need(bool value,string message){if(!value)throw new Exception(message);}
    static void Test(string name,Action action){action();passed++;Console.WriteLine("PASS shared UI: "+name);}
    static void Click(Button button){button.RaiseEvent(new RoutedEventArgs(Button.ClickEvent));}
    static IEnumerable<Element> All(Element root)
    {
#if MACOS
        return root.GetLogicalDescendants().OfType<Element>();
#else
        return LogicalTreeHelper.GetChildren(root).OfType<Element>().SelectMany(c=>new[]{c}.Concat(All(c)));
#endif
    }
    static T Find<T>(Element root,string name) where T:Element { return All(root).OfType<T>().Single(c=>c.Name==name); }
    public static int Run()
    {
#if MACOS
        return RunCore();
#else
        Exception failure=null;var thread=new System.Threading.Thread(()=>{try{RunCore();}catch(Exception e){failure=e;}});
        thread.SetApartmentState(System.Threading.ApartmentState.STA);thread.IsBackground=true;thread.Start();
        Need(thread.Join(60000),"Shared UI tests timed out");if(failure!=null)throw failure;return 0;
#endif
    }
    static string Tip(Button button)
    {
#if MACOS
        return Convert.ToString(ToolTip.GetTip(button));
#else
        return Convert.ToString(button.ToolTip);
#endif
    }
    static string TestBase()
    {
#if MACOS
        // macOS's /var temp alias is a symlink; production intentionally rejects it.
        return Path.GetFullPath(".");
#else
        return Path.GetTempPath();
#endif
    }
    static int RunCore()
    {
        string root=Path.Combine(TestBase(),"tsfp-shared-ui-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        try {
            Test("texture installation is opt-in, exposes both local sources and locks during work",()=>{
                var page=new SettingsPage(new ResourceDictionary(),root,true);page.SelectTab(1);
                int mode=-1;bool cancelled=false;
                page.InstallTextures+=value=>mode=value;page.CancelTextureInstall+=()=>cancelled=true;
                Click(Find<Button>(page.View,"DownloadTexturePack"));Need(mode==0,"Download not routed");
                Click(Find<Button>(page.View,"InstallTextureZip"));Need(mode==1,"ZIP not routed");
                Click(Find<Button>(page.View,"InstallTextureFolder"));Need(mode==2,"Folder not routed");
                page.TextureBusy(true);
                Need(!Find<Button>(page.View,"DownloadTexturePack").IsEnabled,"Busy download remains enabled");
                page.TextureProgress("Installing test pack");
                Need(Find<TextBlock>(page.View,"TexturePackStatus").Text=="Installing test pack","Progress not shown");
                Click(Find<Button>(page.View,"CancelTextureInstall"));Need(cancelled,"Cancel not routed");
                page.TextureBusy(false);Need(Find<Button>(page.View,"InstallTextureZip").IsEnabled,"Installer remained disabled");
            });
            Test("settings drafts, FOV reset, cancel and save",()=>{
                var page=new SettingsPage(new ResourceDictionary(),root,true);page.SelectTab(1);
                var fov=Find<Slider>(page.View,"VerticalFovSlider");var number=Find<TextBlock>(page.View,"VerticalFovValue");
                fov.Value=80;Need(number.Text=="80°","FOV label did not follow slider");
                bool cancelled=false;page.Cancelled+=()=>cancelled=true;Click(Find<Button>(page.View,"SettingsCancelButton"));
                Need(cancelled&&!Directory.Exists(Path.Combine(root,"Settings")),"Cancel saved a draft");
                Click(Find<Button>(page.View,"ResetFovButton"));Need(fov.Value==55,"FOV reset differs");
                fov.Value=75;Click(Find<Button>(page.View,"SettingsSaveButton"));Need(GameSettings.Load(root,null).VerticalFov==75,"Shared page did not save FOV");
            });
            Test("binding capture, conflict cancel, reassignment and clear",()=>{
                var page=new SettingsPage(new ResourceDictionary(),root,true);
                var binding=All(page.View).OfType<Button>().First(b=>Tip(b)==GameSettings.Labels[0]+", binding 1");
                Func<string,Button> action=label=>All(page.View).OfType<Button>().Single(b=>b.Name!="SettingsCancelButton"&&Convert.ToString(b.Content)==label);
                string original=Convert.ToString(binding.Content);Click(binding);Need(Find<Border>(page.View,"BindingCaptureArea")!=null,"No capture prompt");
                Click(action("Bind Escape"));Click(action("Cancel"));Need(Convert.ToString(binding.Content)==original,"Cancelled conflict changed binding");
                Click(binding);Click(action("Bind Escape"));Click(action("Move binding"));Need(Convert.ToString(binding.Content)=="Escape","Conflict move failed");
                Click(Find<Button>(page.View,"SettingsSaveButton"));var saved=GameSettings.Load(root,null);
                Need(GameSettings.KeyCode(saved.Binding(0)[0])==27&&saved.Binding(6).All(t=>GameSettings.KeyCode(t)!=27),"Conflict was not removed from previous action");
                Click(binding);Click(action("Clear binding"));Need(Convert.ToString(binding.Content)=="Unbound","Clear did not update draft");
            });
            Test("Discord uses the same settings model and preserves other settings",()=>{
                var page=new SettingsPage(new ResourceDictionary(),root,true);page.SelectTab(2);
                Find<CheckBox>(page.View,"DiscordPresenceEnabled").IsChecked=true;Find<TextBox>(page.View,"DiscordApplicationId").Text="123456789012345678";
                Click(Find<Button>(page.View,"SettingsSaveButton"));var saved=GameSettings.Load(root,null);
                Need(saved.DiscordEnabled&&saved.DiscordApplicationId=="123456789012345678"&&saved.VerticalFov==75,"Discord save changed other settings");
            });
            Test("both launchers migrate retired timing preferences",()=>{
                string shared=Path.Combine(root,"high-refresh");Directory.CreateDirectory(Path.Combine(shared,"Settings"));
                File.WriteAllText(Path.Combine(shared,"Settings/runtime.conf"),"frame_interp = 2\nframe_target = 144\nrefresh_rate = 120\nframe_cap = 0\n");
                var page=new SettingsPage(new ResourceDictionary(),shared,true);page.SelectTab(1);
                Find<Slider>(page.View,"VerticalFovSlider").Value=70;
                Click(Find<Button>(page.View,"SettingsSaveButton"));var saved=GameSettings.Load(shared,null);
                Need(saved.VerticalFov==70 && saved.PresentationFps==0 && saved.RefreshRate==0 && saved.Interpolation==1 && saved.FrameCap=="60","Retired timing settings survived migration");
            });
            Test("native frame-rate defaults, beta selection and game-menu synchronization",()=>{
                string folder=Path.Combine(root,"native-fps");Directory.CreateDirectory(Path.Combine(folder,"Settings"));
                string sidecar=Path.Combine(folder,"Settings/pc-frame-rate.ini");
                File.WriteAllText(Path.Combine(folder,"Settings/runtime.conf"),"native_fps = 144\nfuture_setting = retained\n");
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(1);
                var picker=Find<ComboBox>(page.View,"NativeFrameRate");
                Need(picker.Items.Count==3 && picker.SelectedIndex==0,"High refresh must remain opt-in");
                Need(Find<TextBlock>(page.View,"NativeFrameRateBetaNote").Text.Contains("Arcade"),"Missing beta limitations");
                picker.SelectedIndex=2;Click(Find<Button>(page.View,"SettingsCancelButton"));
                Need(!File.Exists(sidecar),"Cancelled draft changed the game preference");
                foreach(int choice in new[]{2,1,0}) {
                    picker.SelectedIndex=choice;Click(Find<Button>(page.View,"SettingsSaveButton"));
                    int rate=GameSettings.NativeFrameRates[choice];
                    Need(GameSettings.Load(folder,null).NativeFps==rate,"Saved rate did not round trip");
                    Need(File.ReadAllText(sidecar)=="TSFP_FRAME_RATE_V1 "+rate+"\n","Game-menu sidecar format differs");
                    var info=GameSettings.StartInfo(folder);
                    Need(info.EnvironmentVariables["TSFP_NATIVE_FPS"]==rate.ToString(),"Launch did not select the saved timing mode");
                    Need(info.EnvironmentVariables["RECOMP_FRAME_INTERP"]=="1" && info.EnvironmentVariables["RECOMP_FRAME_TARGET"]=="0" && info.EnvironmentVariables["RECOMP_FPS_CAP"]=="60","Retired presentation settings were reactivated");
                }
                Need(File.ReadAllText(Path.Combine(folder,"Settings/runtime.conf")).Contains("future_setting = retained"),"Unrelated setting lost");
                File.WriteAllText(sidecar,"TSFP_FRAME_RATE_V1 120\n"); // Simulate in-game settings.
                var reopened=new SettingsPage(new ResourceDictionary(),folder,true);reopened.SelectTab(1);
                Need(Find<ComboBox>(reopened.View,"NativeFrameRate").SelectedIndex==1,"In-game choice not reflected");
                Find<Slider>(reopened.View,"VerticalFovSlider").Value=70;Click(Find<Button>(reopened.View,"SettingsSaveButton"));
                Need(GameSettings.Load(folder,null).NativeFps==120,"Unrelated edit reset frame rate");
            });
            Test("native frame-rate rejects invalid or concurrently changed preferences",()=>{
                string folder=Path.Combine(root,"native-fps-conflict");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.Save();
                string sidecar=Path.Combine(folder,"Settings/pc-frame-rate.ini");
                File.WriteAllText(sidecar,"TSFP_FRAME_RATE_V1 144\n");
                bool rejected=false;try{model.NativeFps=120;model.Save();}catch{rejected=true;}
                Need(rejected && File.ReadAllText(sidecar)=="TSFP_FRAME_RATE_V1 144\n","Concurrent game preference was overwritten");
                foreach(string invalid in new[]{"TSFP_FRAME_RATE_V1 60","TSFP_FRAME_RATE_V1 240","TSFP_FRAME_RATE_V2 144","TSFP_FRAME_RATE_V1 144 extra"}) {
                    File.WriteAllText(sidecar,invalid);rejected=false;try{GameSettings.Load(folder,null);}catch{rejected=true;}
                    Need(rejected && File.ReadAllText(sidecar)==invalid,"Invalid sidecar was accepted or rewritten");
                }
            });
            Test("native frame-rate failure rolls back all earlier settings writes",()=>{
                string folder=Path.Combine(root,"native-fps-rollback");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.Save();
                var files=LauncherPlatform.Settings(folder).Select(p=>Core.PathIn(folder,p)).ToArray();
                var before=files.Select(File.ReadAllBytes).ToArray();
                Directory.CreateDirectory(files[4]+".bak");model.NativeFps=144;model.VerticalFov=75;
                bool rejected=false;try{model.Save();}catch{rejected=true;}
                Need(rejected,"Blocked backup should reject save");
                for(int i=0;i<files.Length;i++)Need(File.ReadAllBytes(files[i]).SequenceEqual(before[i]),"Partial settings commit survived");
            });
            Test("news selection, stale notes and toolbar spacing",()=>{
                var page=new WhatsNewPage();page.SetVersions("v1.0.0","1.0.0",false);
                var game=new Release{Tag="v1.1.0",Url=ReleaseClient.ReleasesPage,Notes="## Changes\n- **Shared** release"};
                var launcher=new Release{Tag="1.1.0",Url=ReleaseClient.ReleasesPage+"/tag/launcher-v1.1.0"};
                page.SetNews(new ReleaseNews{Game=game,Launcher=launcher,CheckedUtc=DateTime.UtcNow});
                Need(page.SelectedUrl==game.Url&&page.OnGitHub.IsEnabled,"Game release selection failed");Click(page.LauncherTab);
                Need(page.SelectedUrl==launcher.Url,"Launcher tab did not switch notes");page.SetLoading(true);Need(!page.Refresh.IsEnabled,"Refresh was not locked");page.Failed();
                Need(page.Refresh.IsEnabled&&page.SelectedUrl==launcher.Url&&page.StatusText.Contains("Showing notes checked"),"Failed refresh lost previous notes");
                Need(page.GameTab.Margin.Right>=10&&page.LauncherTab.Margin.Right>=10&&page.Refresh.Margin.Right>=10,"Shared toolbar spacing regressed");
            });
            Test("installed-only notes remain selectable and issue errors retain their age",()=>{
                var page=new WhatsNewPage();page.SetVersions("v1.0.0","1.0.0",false);
                page.SetNews(new ReleaseNews{InstalledGame=new Release{Tag="v1.0.0",Url=ReleaseClient.ReleasesPage},CheckedUtc=DateTime.UtcNow});
                Need(page.Edition.SelectedIndex==1&&page.SelectedUrl!=null,"Installed-only notes were lost");
                page.SetIssues(new KnownIssues{Items=new[]{new KnownIssue{Number=12,Title="Literal <Button>"}},CheckedUtc=DateTime.UtcNow});page.IssuesFailed();
                Need(page.RefreshIssues.IsEnabled&&page.IssuesStatusText.Contains("Showing the list checked"),"Issue refresh failure lost stale status");
            });
            Test("version selection persists across pagination and locks while busy",()=>{
                var page=new GameVersionsPage();page.SetInstalled("v1.0.0");
                page.SetReleases(new GameReleaseList{Releases=new[]{new Release{Tag="v1.0.0",Url=ReleaseClient.ReleasesPage}},HasMore=true},true);
                page.SetReleases(new GameReleaseList{Releases=new[]{new Release{Tag="v1.1.0",Url=ReleaseClient.ReleasesPage}}},false);
                Need(page.Count==2&&page.Selected.Tag=="v1.0.0"&&page.Install.Content.ToString().StartsWith("Reinstall"),"Pagination changed selected version");
                page.SetBusy(true,false);Need(!page.Install.IsEnabled&&!page.Refresh.IsEnabled,"Busy version page accepted changes");page.SetBusy(false,true);Need(!page.Install.IsEnabled,"Running game could be replaced");
                page.SetBusy(false,false);page.SetLoading(true);page.Failed();Need(page.Count==2&&page.Install.IsEnabled,"Failure discarded loaded releases");
            });
            Test("maintenance locks navigation while retaining cancellation",()=>{
                var repair=new MaintenancePage(false,true);var diagnostics=new MaintenancePage(true,true);var area=new MaintenanceArea(repair,diagnostics);
                repair.SetBusy(true,false);area.SetBusy(true);area.Tabs.SelectedIndex=1;
                Need(area.Tabs.SelectedIndex==0&&!repair.VerifyGame.IsEnabled&&repair.Cancel.IsEnabled,"Maintenance did not keep cancel available and lock navigation");
                area.SetBusy(false);area.Tabs.SelectedIndex=1;Need(area.Tabs.SelectedIndex==1,"Maintenance stayed locked");
                repair.SetVerification(new VerificationResult{Version="v1.0.0"},new VerificationResult{Version=LauncherVersion.Value});repair.SetBusy(false,false);
                Need(repair.RepairGame.IsEnabled&&repair.RepairLauncher.IsEnabled,"Verified files cannot be repaired");repair.SetBusy(false,true);Need(!repair.RepairGame.IsEnabled&&!repair.RepairLauncher.IsEnabled,"Repair allowed during play");
            });
            Console.WriteLine(passed+" shared launcher UI groups passed.");return 0;
        }finally{Directory.Delete(root,true);}
    }
}
