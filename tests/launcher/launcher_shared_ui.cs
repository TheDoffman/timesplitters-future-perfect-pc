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
    static Dictionary<string,object> ChannelRecord(string tag,bool preview=false)
    {
        string asset="TimeSplitters-Future-Perfect-PC-"+tag+"-"+LauncherPlatform.AssetSuffix+".zip";
        return Json.Object(Json.Encode(new {tag_name=tag,prerelease=preview,draft=false,body="Notes",assets=new[]{new {
            name=asset,state="uploaded",size=100,digest="sha256:"+new string('a',64),browser_download_url=ReleaseClient.ReleasesPage+"/download/"+tag+"/"+asset}}}));
    }
    static int RunCore()
    {
        string root=Path.Combine(TestBase(),"tsfp-shared-ui-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(root);
        try {
            SaveBackupChecks.Run(root,Test);
            Test("complete language packs save, cancel, launch and restore English",()=>{
                string folder=Path.Combine(root,"languages");Directory.CreateDirectory(folder);
                File.WriteAllText(Path.Combine(folder,"BUILD.json"),"{\"language_packs\":true}");
                string pak=Core.PathIn(folder,"Data/pak");Directory.CreateDirectory(pak);
                foreach(string suffix in new[]{"e","f","g","i","s"})File.WriteAllText(Path.Combine(pak,"sounds_"+suffix+".pak"),"P5CK fixture "+suffix);
                var before=Directory.GetFiles(pak).ToDictionary(f=>f,File.ReadAllBytes);
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(3);
                var picker=Find<ComboBox>(page.View,"LanguagePack");var save=Find<Button>(page.View,"SettingsSaveButton");
                Need(picker.Items.Count==5 && picker.SelectedIndex==0,"Language default or choices differ");
                picker.SelectedIndex=1;Click(Find<Button>(page.View,"SettingsCancelButton"));
                Need(!File.Exists(Core.PathIn(folder,LauncherPlatform.Settings(folder)[0])),"Cancelled language was saved");
                foreach(int i in new[]{1,2,3,4,0}) {
                    picker.SelectedIndex=i;Click(save);
                    Need(!save.IsEnabled,"Language draft remains dirty");
                    Need(GameSettings.Load(folder,null).LanguagePack==GameSettings.LanguagePacks[i],"Language did not round trip");
                    var info=GameSettings.StartInfo(folder);
                    Need(info.EnvironmentVariables["TSFP_GAME_LANGUAGE"]==GameSettings.LanguagePacks[i],"Launch lost selected language");
                    picker.SelectedIndex=(i+1)%5;Need(save.IsEnabled,"Language edit did not enable Save");
                    picker.SelectedIndex=i;Need(!save.IsEnabled,"Reverted language remains dirty");
                }
                var reopened=new SettingsPage(new ResourceDictionary(),folder,true);
                Need(Find<ComboBox>(reopened.View,"LanguagePack").SelectedIndex==0,"Return to English was not retained");
                foreach(var entry in before)Need(File.ReadAllBytes(entry.Key).SequenceEqual(entry.Value),"Language selection changed original pack bytes");
            });
            Test("language launch rejects missing packs and unsupported game versions",()=>{
                string folder=Path.Combine(root,"language-errors");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.LanguagePack="fr";model.Save();
                bool rejected=false;try{GameSettings.StartInfo(folder);}catch(Exception e){rejected=e.Message.Contains("missing");}
                Need(rejected,"Missing French archive was silently ignored");
                string pak=Core.PathIn(folder,"Data/pak/sounds_f.pak");Directory.CreateDirectory(Path.GetDirectoryName(pak));File.WriteAllText(pak,"invalid");
                rejected=false;try{GameSettings.StartInfo(folder);}catch(Exception e){rejected=e.Message.Contains("invalid");}
                Need(rejected && File.ReadAllText(pak)=="invalid","Invalid pack was accepted or modified");
                File.WriteAllText(pak,"P5CK fixture");File.WriteAllText(Core.PathIn(folder,"BUILD.json"),"{\"version\":\"v0.4.0\",\"auto_display_resolution\":true}");
                Need(GameSettings.Load(folder,null).LanguagePack=="fr","Older release erased saved choice");
                rejected=false;try{GameSettings.StartInfo(folder);}catch(Exception e){rejected=e.Message.Contains("Update the game");}
                Need(rejected,"Old game silently ignored language choice");
                model=GameSettings.Load(folder,null);model.LanguagePack="en";model.Save();
                Need(GameSettings.StartInfo(folder).EnvironmentVariables["TSFP_GAME_LANGUAGE"]=="en","English failed on an older game");
            });
            Test("language preferences retain concurrent edits and reject unknown codes",()=>{
                string folder=Path.Combine(root,"language-conflict");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.Save();var concurrent=GameSettings.Load(folder,null);concurrent.LanguagePack="de";concurrent.Save();
                model.LanguagePack="es";bool rejected=false;try{model.Save();}catch{rejected=true;}
                Need(rejected && GameSettings.Load(folder,null).LanguagePack=="de","Concurrent language was overwritten");
                string file=Core.PathIn(folder,LauncherPlatform.Settings(folder)[0]);
                string bad=File.ReadAllText(file).Replace("\"de\"","\"../fr\"");File.WriteAllText(file,bad);
                rejected=false;try{GameSettings.Load(folder,null);}catch{rejected=true;}
                Need(rejected && File.ReadAllText(file)==bad,"Invalid language was accepted or rewritten");
            });
            Test("save manager lists dated restore points and requires restore confirmation",()=>{
                string folder=Path.Combine(root,"saves-page");Directory.CreateDirectory(folder);
                string saves=Core.PathIn(folder,LauncherPlatform.Saves(folder));Directory.CreateDirectory(saves);File.WriteAllText(Path.Combine(saves,"profile"),"original");
                var backup=SaveBackups.Create(folder,"Manual",null,System.Threading.CancellationToken.None);File.WriteAllText(Path.Combine(saves,"profile"),"current");
                var page=new SavesPage(folder,false);
                Need(Find<Button>(page.View,"ChooseSaveFolder")!=null && Find<Button>(page.View,"DefaultSaveFolder")!=null,"Save location controls missing");
                Need(Find<TextBlock>(page.View,"ActiveSaveLocation").Text.Contains(saves),"Active save folder missing");
                Click(Find<Button>(page.View,"DefaultSaveFolder"));
                Need(Find<Button>(page.View,"UseSaveLocation")!=null && Find<Button>(page.View,"CopySaveLocation")!=null,"Folder selection must offer use or copy");
                Click(Find<Button>(page.View,"CancelSaveLocation"));
                Need(page.Choices.Items.Count==1 && ((SaveBackup)page.Choices.SelectedItem).Id==backup.Id && page.Restore.IsEnabled,"Backup list or restore selection missing");
                Click(page.Restore);Need(Find<Button>(page.View,"ConfirmSaveRestore")!=null,"Restore did not ask for confirmation");
                Click(Find<Button>(page.View,"CancelSaveRestore"));Need(File.ReadAllText(Path.Combine(saves,"profile"))=="current","Cancelled restore changed saves");
                var preview=new SavesPage(folder,true);Click(preview.Backup);Need(SaveBackups.List(folder).Length==1,"Preview created a real backup");
            });
            Test("texture verification reports missing and changed parts without modifying custom files",()=>{
                string folder=Path.Combine(root,"texture-health");Directory.CreateDirectory(Path.Combine(folder,"Textures"));
                string texture=Core.PathIn(folder,"Textures/0123456789abcdef.dds"),meta=Path.ChangeExtension(texture,".json");
                File.WriteAllText(texture,"texture fixture");File.WriteAllText(meta,"metadata fixture");File.WriteAllText(Core.PathIn(folder,"Textures/custom.dds"),"custom");
                var pack=new TextureRelease {Version="0.1.0-beta.1",Count=2};pack.Parts.Add(new Release{AssetName="textures-001.zip",Size=111});pack.Parts.Add(new Release{AssetName="textures-002.zip",Size=222});
                pack.Files.Add("textures-001.zip",new Dictionary<string,string>{{Path.GetFileName(texture),Core.Hash(texture)},{Path.GetFileName(meta),Core.Hash(meta)}});
                pack.Files.Add("textures-002.zip",new Dictionary<string,string>{{"fedcba9876543210.dds",Core.Hash(texture)},{"fedcba9876543210.json",Core.Hash(meta)}});
                var report=TextureDownload.Verify(folder,pack,System.Threading.CancellationToken.None);
                Need(report.Matched==2 && report.Missing==2 && report.Changed==0 && report.DownloadBytes==222,"Missing-file verification or remaining size differs");
                File.WriteAllText(texture,"edited");report=TextureDownload.Verify(folder,pack,System.Threading.CancellationToken.None);
                Need(report.Changed==1 && report.DownloadBytes==333 && File.ReadAllText(texture)=="edited" && File.ReadAllText(Core.PathIn(folder,"Textures/custom.dds"))=="custom","Verification changed files or missed modified content");
                Core.Write(Core.PathIn(folder,".launcher/texture-pack.json"),"{\"version\":\"0.1.0-beta.1\"}");
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(1);
                var details=Find<TextBlock>(page.View,"TexturePackDetails");Need(details.Text.Contains("Installed pack: 0.1.0-beta.1") && details.Text.Contains("Original textures"),"Installed version or saved mode missing");
                Find<CheckBox>(page.View,"EnhancedTextures").IsChecked=true;Need(details.Text.Contains("unsaved changes"),"Unsaved texture selection not identified");
                Click(Find<Button>(page.View,"SettingsSaveButton"));Need(details.Text.Contains("Next launch (saved): Enhanced textures (Beta)") && !details.Text.Contains("unsaved changes"),"Saved texture mode not refreshed");
                Click(Find<Button>(page.View,"VerifyTexturePack"));Need(Find<TextBlock>(page.View,"TexturePackStatus").Text.Contains("Preview only"),"Preview verification performed live work");
                Core.Write(Core.PathIn(folder,".launcher/texture-pack.json"),"broken receipt");
                Need(TextureDownload.InstalledVersion(folder).Contains("attention") && File.ReadAllText(Core.PathIn(folder,".launcher/texture-pack.json"))=="broken receipt","Malformed receipt blocked settings or was overwritten");
                Core.Write(Core.PathIn(folder,".launcher/texture-pack.json"),new string('x',5000));Need(TextureDownload.InstalledVersion(folder).Contains("attention"),"Damaged receipt blocked settings or hid the error");
            });
            Test("Preview groups named feature tracks and remembers the selected test across Stable",()=>{
                var picker=new GameChannelPickerPage();int changes=0;picker.Changed+=()=>changes++;
                var releases=new[]{new Release {Tag="v0.4.0-beta.1",Channel="beta",ChannelName="Beta"},
                    new Release {Tag="v0.4.0-story-coop.1",Channel="story-coop",ChannelName="Story Co-op Preview"}};
                picker.SetReleases(releases,"stable","story-coop");
                Need(picker.Channel.Items.Count==2&&picker.Selected=="stable"&&!picker.ShowingTracks&&changes==0,"Top-level channels must be Stable and Preview only");
                picker.Channel.SelectedIndex=1;
                Need(picker.ShowingTracks&&picker.Selected=="story-coop"&&changes==1,"Preview did not restore the remembered feature track");
                Need(picker.Track.Items.Cast<GameChannelChoice>().Select(c=>c.Name).SequenceEqual(new[]{"Beta","Story Co-op"}),"Named tests were not grouped under Preview");
                picker.Track.SelectedItem=picker.Track.Items.Cast<GameChannelChoice>().First(c=>c.Id=="beta");
                Need(picker.Selected=="beta"&&changes==2,"Beta did not select its own track");
                picker.Channel.SelectedIndex=0;picker.Channel.SelectedIndex=1;
                Need(picker.Selected=="beta"&&changes==4,"Switching through Stable lost the selected preview track");
                picker.SetReleases(new Release[0],"story-coop");
                Need(picker.Selected=="story-coop"&&picker.ShowingTracks&&changes==4,"Unavailable tracks must remain selected without triggering checks");
                picker.SetEnabled(false);Need(!picker.Channel.IsEnabled&&!picker.Track.IsEnabled,"Busy state must lock both selectors");
            });
            Test("game channels select independent series and permit explicit return to Stable",()=>{
                var records=new[]{ChannelRecord("v0.4.1"),ChannelRecord("v0.5.0-story-coop.2",true),ChannelRecord("v0.6.0-other.8",true),ChannelRecord("v0.5.0-beta.1",true),ChannelRecord("v0.3.1-alpha.2",true),ChannelRecord("v0.6.0-preview.99",true)};
                var releases=ReleaseClient.GameVersions(Json.Encode(records)).Releases;
                Need(GameChannels.Select(releases,"story-coop").Tag=="v0.5.0-story-coop.2","Experiment escaped its channel");
                Need(GameChannels.Select(releases,"beta").Tag=="v0.5.0-beta.1","Beta absorbed experiments");
                var stable=GameChannels.Select(releases,"stable");Need(stable.Tag=="v0.4.1","Stable absorbed preview");
                Need(GameChannels.CanInstall(stable,"v0.5.0-story-coop.2"),"Returning to Stable must allow a lower version");
                Need(!GameChannels.CanInstall(stable,"v0.4.2"),"Same-channel downgrade must remain explicit history selection");
                Need(releases.Any(r=>r.Tag=="v0.3.1-alpha.2"&&r.Channel=="legacy"&&r.ChannelName=="Legacy Preview"),"Historical alpha was relabeled Beta");
                Need(releases.Any(r=>r.Tag=="v0.6.0-preview.99"&&r.Channel=="legacy"),"Historical preview was relabeled Beta");
                Need(!GameChannels.Choices(releases,"beta").Any(c=>c.Id=="legacy"),"Legacy versions must stay in history, outside testing tracks");
                Need(GameChannels.Select(releases,"withdrawn")==null,"Missing experiment must not substitute Stable");
                Need(GameChannels.Choices(releases,"withdrawn").Any(c=>c.Id=="withdrawn"),"Unavailable selected channel was lost");
            });
            Test("Story co-op beta joins its existing track while retaining exact tag metadata",()=>{
                var record=ChannelRecord("v0.5.0-story-coop-beta.1",true);
                var metadata=new Dictionary<string,object>{{"schema",1},{"channel","story-coop-beta"},{"name","Story Co-op Beta Preview"},{"source_commit",new string('a',40)},{"minimum_launcher","1.3.0"}};
                record["body"]="<!-- tsfp-release "+Json.Encode(metadata)+" -->\nBeta notes";
                var releases=ReleaseClient.GameVersions(Json.Encode(new[]{record,ChannelRecord("v0.4.0-story-coop.1",true),ChannelRecord("v0.5.0-beta.1",true),ChannelRecord("v0.6.0-other-beta.1",true)})).Releases;
                var selected=GameChannels.Select(releases,"story-coop");
                Need(selected.Tag=="v0.5.0-story-coop-beta.1"&&selected.Channel=="story-coop"&&selected.ChannelName=="Story Co-op Preview","Beta was split from the existing Story track");
                Need(GameChannels.Select(releases,"story-coop-beta")==selected,"Saved beta track did not resolve to Story co-op");
                var choices=GameChannels.Choices(releases,"story-coop-beta");
                Need(choices.Count(c=>c.Id=="story-coop")==1&&!choices.Any(c=>c.Id=="story-coop-beta"),"Duplicate Story tracks were offered");
                Need(GameChannels.Select(releases,"beta").Tag=="v0.5.0-beta.1"&&GameChannels.Select(releases,"other-beta").Tag=="v0.6.0-other-beta.1","Unrelated beta tracks were merged");
                Need(GameChannels.FromTag(selected.Tag)=="story-coop"&&!GameChannels.CanInstall(selected,selected.Tag),"Installed beta identity was lost");
                metadata["channel"]="story-coop";record["body"]="<!-- tsfp-release "+Json.Encode(metadata)+" -->";
                Need(ReleaseClient.GameVersions(Json.Encode(new[]{record})).Releases.Length==0,"Alias accepted metadata that did not match its immutable tag");
            });
            Test("Story beta preferences migrate without writing on read or losing unrelated settings",()=>{
                string folder=Path.Combine(root,"story-beta-channels");Directory.CreateDirectory(folder);
                string path=Core.PathIn(folder,".launcher/preferences.json");
                Core.Write(path,Json.Encode(new {game_channel="story-coop-beta",game_preview_track="story-coop-beta",launcher_preview=true,custom="keep"}));
                string original=File.ReadAllText(path);
                Need(GameChannels.Read(folder)=="story-coop"&&GameChannels.ReadPreviewTrack(folder)=="story-coop"&&File.ReadAllText(path)==original,"Reading beta preferences changed the file or lost its track");
                GameChannels.Save(folder,"story-coop-beta");var prefs=Json.Object(File.ReadAllText(path));
                Need(Json.String(prefs,"game_channel")=="story-coop"&&Json.String(prefs,"game_preview_track")=="story-coop"&&Json.String(prefs,"custom")=="keep"&&SelfUpdate.PreviewChannel(folder),"Migrating Story beta lost preferences");
                GameChannels.Save(folder,"stable","story-coop-beta");
                Need(GameChannels.Read(folder)=="stable"&&GameChannels.ReadPreviewTrack(folder)=="story-coop","Stable lost the remembered Story track");
            });
            Test("game preview preferences migrate without enrolling launcher previews or losing unrelated preferences",()=>{
                string folder=Path.Combine(root,"channels");Directory.CreateDirectory(folder);
                Need(GameChannels.Read(folder)=="stable","Fresh installs must use Stable");
                string path=Core.PathIn(folder,".launcher/preferences.json");
                Core.Write(path,Json.Encode(new {preview=true,custom="keep"}));
                Need(GameChannels.Read(folder)=="beta"&&!SelfUpdate.PreviewChannel(folder),"Old Preview must migrate to Beta without enrolling launcher previews");
                Core.Write(path,Json.Encode(new {game_channel="preview",game_preview_track="preview",custom="keep"}));
                Need(GameChannels.Read(folder)=="beta"&&GameChannels.ReadPreviewTrack(folder)=="beta","Explicit general-preview preferences were not migrated");
                Core.Write(path,Json.Encode(new {game_channel="stable",game_preview_track="preview",custom="keep"}));
                Need(GameChannels.Read(folder)=="stable"&&GameChannels.ReadPreviewTrack(folder)=="beta","Stable must retain a migrated Beta track");
                GameChannels.Save(folder,"story-coop");var prefs=Json.Object(File.ReadAllText(path));
                Need(GameChannels.Read(folder)=="story-coop"&&GameChannels.ReadPreviewTrack(folder)=="story-coop"&&Json.String(prefs,"custom")=="keep"&&!Json.Flag(prefs,"preview"),"Channel save lost preferences");
                GameChannels.Save(folder,"stable");
                Need(GameChannels.Read(folder)=="stable"&&GameChannels.ReadPreviewTrack(folder)=="story-coop","Stable lost the remembered preview track");
                bool rejected=false;try{GameChannels.Save(folder,"../outside");}catch(InvalidDataException){rejected=true;}Need(rejected,"Unsafe channel ID accepted");
            });
            Test("release channel metadata is checked and hidden from visible notes",()=>{
                var record=ChannelRecord("v0.5.0-story-coop.1",true);
                var metadata=new Dictionary<string,object>{{"schema",1},{"channel","story-coop"},{"name","Story Co-op Preview"},{"source_commit",new string('a',40)},{"minimum_launcher","1.3.0"}};
                record["body"]="<!-- tsfp-release "+Json.Encode(metadata)+" -->\nHuman release notes";
                var release=ReleaseClient.GameVersions(Json.Encode(new[]{record})).Releases.Single();
                Need(release.ChannelName=="Story Co-op Preview"&&release.MinimumLauncher=="1.3.0"&&!release.Notes.Contains("tsfp-release"),"Metadata was lost or displayed");
                record["body"]="<!-- tsfp-release {bad json} -->";
                Need(ReleaseClient.GameVersions(Json.Encode(new[]{record})).Releases.Length==0,"Malformed metadata was accepted");
                metadata["channel"]="other";record["body"]="<!-- tsfp-release "+Json.Encode(metadata)+" -->";
                Need(ReleaseClient.GameVersions(Json.Encode(new[]{record})).Releases.Length==0,"Mismatched metadata was accepted");
            });
            Test("optional texture beta toggle saves a launch preference without changing textures",()=>{
                string folder=Path.Combine(root,"textures-toggle");Directory.CreateDirectory(Path.Combine(folder,"Textures"));
                string texture=Path.Combine(folder,"Textures/0123456789abcdef.dds");File.WriteAllText(texture,"untouched texture fixture");
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(1);
                var toggle=Find<CheckBox>(page.View,"EnhancedTextures");
                Need(toggle.IsChecked==false && Convert.ToString(toggle.Content).Contains("Beta"),"Classic default or Beta label missing");
                Need(!Find<Button>(page.View,"DownloadTexturePack").IsEnabled,"Download needs a checked manifest first");
                Need(Find<CheckBox>(page.View,"TextureUpdateChecks").IsChecked==false,"Fresh install unexpectedly opted in");
                Click(Find<Button>(page.View,"CheckTexturePack"));
                Need(Find<TextBlock>(page.View,"TexturePackStatus").Text.Contains("Preview only"),"Preview initiated a network check");
                Need(Find<TextBlock>(page.View,"EnhancedTexturesHint").Text.Contains("incomplete"),"Missing beta limitation");
                toggle.IsChecked=true;Click(Find<Button>(page.View,"SettingsCancelButton"));
                Need(!Directory.Exists(Path.Combine(folder,"Settings")),"Cancelled texture draft was saved");
                foreach(bool enabled in new[]{true,false}) {
                    toggle.IsChecked=enabled;Click(Find<Button>(page.View,"SettingsSaveButton"));
                    Need(GameSettings.Load(folder,null).EnhancedTextures==enabled,"Texture preference did not persist");
                    Need(GameSettings.StartInfo(folder).EnvironmentVariables["TSFP_TEXTURE_MODE"]==(enabled?"4k":"classic"),"Runtime texture mode differs from saved choice");
                    Need(File.ReadAllText(texture)=="untouched texture fixture","Toggle modified texture assets");
                    var reopened=new SettingsPage(new ResourceDictionary(),folder,true);reopened.SelectTab(1);
                    Need(Find<CheckBox>(reopened.View,"EnhancedTextures").IsChecked==enabled,"Reopened toggle lost saved choice");
                }
                var saved=GameSettings.Load(folder,null);saved.EnhancedTextures=true;saved.NativeFps=120;saved.Save();
                saved=GameSettings.Load(folder,null);saved.VerticalFov=70;saved.Save();
                Need(GameSettings.Load(folder,null).EnhancedTextures && GameSettings.Load(folder,null).NativeFps==120,"Unrelated save reset preferences");
            });
            Test("texture preference works without a pack and rejects malformed saved choices",()=>{
                string folder=Path.Combine(root,"textures-no-pack");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.EnhancedTextures=true;model.Save();
                Need(GameSettings.StartInfo(folder).EnvironmentVariables["TSFP_TEXTURE_MODE"]=="4k","Missing pack blocked selection instead of runtime fallback");
                Need(!Directory.Exists(Path.Combine(folder,"Textures")),"Saving tried to install textures");
                string controls=Core.PathIn(folder,LauncherPlatform.Settings(folder)[0]);
                var doc=Json.Object(File.ReadAllText(controls));((Dictionary<string,object>)doc["launcher"])["enhanced_textures"]="yes";
                string invalid=Json.Encode(doc);File.WriteAllText(controls,invalid);bool rejected=false;
                try {GameSettings.Load(folder,null);}catch{rejected=true;}
                Need(rejected && File.ReadAllText(controls)==invalid,"Malformed preference was accepted or rewritten");
            });
            Test("saved settings disable Save until a persisted draft changes",()=>{
                string folder=Path.Combine(root,"save-button");Directory.CreateDirectory(folder);
                var page=new SettingsPage(new ResourceDictionary(),folder,true);
                var save=Find<Button>(page.View,"SettingsSaveButton");int saves=0;page.Saved+=()=>saves++;
                Click(save);Need(!save.IsEnabled && saves==1,"Successful save did not disable Save");
                Click(save);Need(saves==1,"Disabled Save rewrote settings");
                for(int tab=0;tab<3;tab++){page.SelectTab(tab);Need(!save.IsEnabled,"Tab navigation enabled Save");}
                var flags=System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic;
                Func<string,object> field=name=>page.GetType().GetField(name,flags).GetValue(page);
                foreach(string name in new[]{"sensitivity","width","height","discordId"}) {
                    var input=(TextBox)field(name);string before=input.Text;
                    input.Text=before+"0";Need(save.IsEnabled,name+" did not enable Save");
                    input.Text=before;Need(!save.IsEnabled,name+" reverted draft remained dirty");
                }
                foreach(string name in new[]{"enabled","invert","fullscreen","vrr","fps","skipSplashes","discordEnabled","enhancedTextures"}) {
                    var check=(CheckBox)field(name);bool? before=check.IsChecked;
                    check.IsChecked=before!=true;Need(save.IsEnabled,name+" did not enable Save");
                    check.IsChecked=before;Need(!save.IsEnabled,name+" reverted draft remained dirty");
                }
                foreach(string name in new[]{"device","scale","filtering","frameRate"}) {
                    var choice=(ComboBox)field(name);int before=choice.SelectedIndex;
                    choice.SelectedIndex=(before+1)%choice.Items.Count;Need(save.IsEnabled,name+" did not enable Save");
                    choice.SelectedIndex=before;Need(!save.IsEnabled,name+" reverted draft remained dirty");
                }
                var fov=(Slider)field("fov");double original=fov.Value;
                fov.Value=original+1;Need(save.IsEnabled,"FOV did not enable Save");
                fov.Value=original;Need(!save.IsEnabled,"Reverted FOV remained dirty");
                var automatic=(CheckBox)field("textureUpdates");automatic.IsChecked=automatic.IsChecked!=true;
                Need(!save.IsEnabled,"Immediate texture-check preference enabled deferred Save");
                var inputSensitivity=(TextBox)field("sensitivity");string valid=inputSensitivity.Text;
                inputSensitivity.Text="invalid";Click(save);
                Need(save.IsEnabled && saves==1,"Failed validation disabled Save or reported success");
                inputSensitivity.Text=valid;Need(!save.IsEnabled,"Restored saved input remained dirty");
                page.SelectTab(0);var binding=All(page.View).OfType<Button>().First(b=>Tip(b)==GameSettings.Labels[0]+", binding 1");
                Click(binding);Click(All(page.View).OfType<Button>().Single(b=>Convert.ToString(b.Content)=="Clear binding"));
                Need(save.IsEnabled,"Binding change did not enable Save");Click(save);
                Need(!save.IsEnabled && saves==2 && GameSettings.Load(folder,null).Binding(0)[0]=="none","Binding save did not disable Save or persist");
                Click(All(page.View).OfType<Button>().Single(b=>Convert.ToString(b.Content)=="Restore default controls"));
                Click(All(page.View).OfType<Button>().Single(b=>Convert.ToString(b.Content)=="Restore defaults"));
                Need(save.IsEnabled,"Restoring default controls did not enable Save");Click(save);
                Need(!save.IsEnabled && saves==3,"Saving defaults did not disable Save");
                string controls=Core.PathIn(folder,LauncherPlatform.Settings(folder)[0]);
                string external=File.ReadAllText(controls)+" ";File.WriteAllText(controls,external);
                fov.Value=original+2;Click(save);
                Need(save.IsEnabled && saves==3 && File.ReadAllText(controls)==external,"Conflicting save disabled retry or overwrote external settings");
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
            Test("beta audio opt-in, draft state and game-menu synchronization",()=>{
                string folder=Path.Combine(root,"enhanced-audio");Directory.CreateDirectory(folder);
                string sidecar=Path.Combine(folder,"Settings/pc-audio.ini");
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(3);
                var toggle=Find<CheckBox>(page.View,"EnhancedAudio");
                Need(toggle.IsChecked==false,"Enhanced Audio must default OFF");
                Need(Convert.ToString(toggle.Content).Contains("Beta"),"Audio toggle lacks beta label");
                Need(Find<TextBlock>(page.View,"EnhancedAudioBetaNote").Text.Contains("game build"),"Compatibility hint missing");
                toggle.IsChecked=true;Click(Find<Button>(page.View,"SettingsSaveButton"));
                Need(File.ReadAllText(sidecar)=="TSFP_AUDIO_V1 1\n" && GameSettings.Load(folder,null).EnhancedAudio,"Launcher/game formats differ");
                var save=Find<Button>(page.View,"SettingsSaveButton");Need(!save.IsEnabled,"Saved audio draft remains dirty");
                toggle.IsChecked=false;Need(save.IsEnabled,"Audio edit did not enable Save");Click(save);
                Need(File.ReadAllText(sidecar)=="TSFP_AUDIO_V1 0\n","Audio OFF did not save");
                File.WriteAllText(sidecar,"TSFP_AUDIO_V1 1\n"); // Simulate the in-game menu.
                var reopened=new SettingsPage(new ResourceDictionary(),folder,true);reopened.SelectTab(3);
                Need(Find<CheckBox>(reopened.View,"EnhancedAudio").IsChecked==true,"In-game audio choice not reflected");
                var model=GameSettings.Load(folder,null);model.Sensitivity=.2;model.Save();
                Need(GameSettings.Load(folder,null).EnhancedAudio,"Unrelated save reset the audio choice");
            });
            Test("beta audio invalid and concurrent preferences are preserved",()=>{
                string folder=Path.Combine(root,"enhanced-audio-conflict");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.Save();
                string sidecar=Path.Combine(folder,"Settings/pc-audio.ini");
                File.WriteAllText(sidecar,"TSFP_AUDIO_V1 1\n");
                bool rejected=false;try{model.Save();}catch{rejected=true;}
                Need(rejected && File.ReadAllText(sidecar)=="TSFP_AUDIO_V1 1\n","Concurrent in-game change was overwritten");
                foreach(string invalid in new[]{"TSFP_AUDIO_V1 2","TSFP_AUDIO_V2 1","TSFP_AUDIO_V1 1 extra"}) {
                    File.WriteAllText(sidecar,invalid);rejected=false;try{GameSettings.Load(folder,null);}catch{rejected=true;}
                    Need(rejected && File.ReadAllText(sidecar)==invalid,"Invalid audio preference was accepted or rewritten");
                }
            });
            Test("beta audio failure rolls back the settings transaction",()=>{
                string folder=Path.Combine(root,"enhanced-audio-rollback");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.Save();
                var files=LauncherPlatform.Settings(folder).Select(p=>Core.PathIn(folder,p)).ToArray();
                var before=files.Select(File.ReadAllBytes).ToArray();
                Directory.CreateDirectory(files[5]+".bak");model.EnhancedAudio=true;model.VerticalFov=75;
                bool rejected=false;try{model.Save();}catch{rejected=true;}
                Need(rejected,"Blocked audio backup should reject Save");
                for(int i=0;i<files.Length;i++)Need(File.ReadAllBytes(files[i]).SequenceEqual(before[i]),"Partial settings commit survived audio failure");
            });
            Test("native frame-rate defaults, beta selection and game-menu synchronization",()=>{
                string folder=Path.Combine(root,"native-fps");Directory.CreateDirectory(Path.Combine(folder,"Settings"));
                string sidecar=Path.Combine(folder,"Settings/pc-frame-rate.ini");
                File.WriteAllText(Path.Combine(folder,"Settings/runtime.conf"),"native_fps = 144\nfuture_setting = retained\n");
                var page=new SettingsPage(new ResourceDictionary(),folder,true);page.SelectTab(1);
                var picker=Find<ComboBox>(page.View,"NativeFrameRate");
                Need(picker.Items.Count==4 && picker.SelectedIndex==0,"High refresh must remain opt-in");
                Need(Find<TextBlock>(page.View,"NativeFrameRateBetaNote").Text.Contains("Arcade"),"Missing beta limitations");
                picker.SelectedIndex=2;Click(Find<Button>(page.View,"SettingsCancelButton"));
                Need(!File.Exists(sidecar),"Cancelled draft changed the game preference");
                foreach(int choice in new[]{3,2,1,0}) {
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
                Need(GameSettings.Load(folder,null).NativeFps==120,"Legacy 144 preference did not migrate to 120");
                bool rejected=false;try{model.NativeFps=120;model.Save();}catch{rejected=true;}
                Need(rejected && File.ReadAllText(sidecar)=="TSFP_FRAME_RATE_V1 144\n","Concurrent game preference was overwritten");
                foreach(string invalid in new[]{"TSFP_FRAME_RATE_V1 60","TSFP_FRAME_RATE_V1 300","TSFP_FRAME_RATE_V2 144","TSFP_FRAME_RATE_V1 144 extra"}) {
                    File.WriteAllText(sidecar,invalid);rejected=false;try{GameSettings.Load(folder,null);}catch{rejected=true;}
                    Need(rejected && File.ReadAllText(sidecar)==invalid,"Invalid sidecar was accepted or rewritten");
                }
            });
            Test("native frame-rate failure rolls back all earlier settings writes",()=>{
                string folder=Path.Combine(root,"native-fps-rollback");Directory.CreateDirectory(folder);
                var model=GameSettings.Load(folder,null);model.Save();
                var files=LauncherPlatform.Settings(folder).Select(p=>Core.PathIn(folder,p)).ToArray();
                var before=files.Select(File.ReadAllBytes).ToArray();
                Directory.CreateDirectory(files[4]+".bak");model.NativeFps=240;model.VerticalFov=75;
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
            Test("capture health distinguishes current, stale, missing and blocked capture",()=>{
                string folder=Path.Combine(root,"capture-health");Directory.CreateDirectory(folder);
                Need(CrashReports.CaptureHealth(folder).Contains("Game not installed"),"Missing game claimed support");
                string game=Core.PathIn(folder,LauncherPlatform.GamePath(folder));Directory.CreateDirectory(Path.GetDirectoryName(game));File.WriteAllText(game,"fixture");
#if !MACOS
                Need(CrashReports.CaptureHealth(folder).Contains("helper missing"),"Missing helper claimed support");
                File.WriteAllText(Path.Combine(Path.GetDirectoryName(game),"tsfp_crash_helper.exe"),"fixture");
#endif
                Need(CrashReports.CaptureHealth(folder).Contains("no tracked game"),"Idle helper presence claimed live health");
                string id=GameSessions.Begin(folder,"20261024-120000-000");
                using(var process=System.Diagnostics.Process.GetCurrentProcess()) {
                    GameSessions.Started(folder,id,process);
                    Need(CrashReports.CaptureHealth(folder).Contains("has not confirmed"),"Old game claimed a handshake");
                    string path=Core.PathIn(folder,"Logs/Crashes/capture-health.json");
                    Action<string,int,bool> record=(session,pid,armed)=>File.WriteAllText(path,Json.Encode(new {schema=1,session_id=session,pid=process.Id,helper_pid=pid,armed=armed}));
                    record(id,process.Id,true);Need(CrashReports.CaptureHealth(folder).Contains("Helper responding"),"Valid current heartbeat rejected");
                    Need(InstallationMaintenance.Report(folder,null,null,"fixture").Contains("Freeze monitoring armed"),"Diagnostics omitted health");
                    record(id,process.Id,false);Need(CrashReports.CaptureHealth(folder).Contains("Waiting for guest"),"Unarmed capture called armed");
                    record(new string('e',32),process.Id,true);Need(CrashReports.CaptureHealth(folder).Contains("no matching"),"Old session heartbeat accepted");
                    record(id,process.Id,true);File.SetLastWriteTimeUtc(path,DateTime.UtcNow.AddSeconds(-30));Need(CrashReports.CaptureHealth(folder).Contains("stale"),"Stale helper accepted");
                    record(id,process.Id,true);File.SetLastWriteTimeUtc(path,DateTime.UtcNow.AddMinutes(2));Need(CrashReports.CaptureHealth(folder).Contains("stale"),"Future-dated helper accepted");
                    record(id,Int32.MaxValue,true);Need(CrashReports.CaptureHealth(folder).Contains("Degraded"),"Dead helper accepted");
                    record(id,process.Id,true);
                    string marker=Core.PathIn(folder,".launcher/game-session.json");var current=Json.Object(File.ReadAllText(marker));
                    current["game_started_ticks"]=(process.StartTime.ToUniversalTime().Ticks+1).ToString();File.WriteAllText(marker,Json.Encode(current));
                    Need(!CrashReports.CaptureHealth(folder).Contains("Helper responding"),"Reused game PID accepted");
                    current["game_started_ticks"]=process.StartTime.ToUniversalTime().Ticks.ToString();File.WriteAllText(marker,Json.Encode(current));
                    File.WriteAllText(path,"private path and malformed JSON");Need(!CrashReports.CaptureHealth(folder).Contains("private path"),"Health leaked raw metadata");
                    File.WriteAllText(path,new string('x',2049));Need(CrashReports.CaptureHealth(folder).Contains("invalid"),"Oversized record accepted");
                    File.Delete(path);Directory.Delete(Path.GetDirectoryName(path));File.WriteAllText(Path.GetDirectoryName(path),"blocked storage");
                    Need(CrashReports.CaptureHealth(folder).Contains("storage unavailable"),"Blocked storage claimed writable");
                }
            });
            Test("breadcrumb exports are bounded and preserve unknown files",()=>{
                string folder=Path.Combine(root,"breadcrumb-export");Directory.CreateDirectory(folder);
                string name="crash-20261024-120000000-0000000001-123456";
                string report=CrashReports.Folder(folder,name);Directory.CreateDirectory(report);
                File.WriteAllText(Path.Combine(report,"capture.lock"),"");
                File.WriteAllText(Path.Combine(report,"report.json"),"{\"schema\":1,\"kind\":\"suspected_hang\"}");
                File.WriteAllText(Path.Combine(report,"breadcrumbs.txt"),"1 0 scene-id 4\n");
                File.WriteAllText(Path.Combine(report,"private-file"),"private");
                string zip=Path.Combine(folder,"export.zip");CrashReports.Export(folder,name,zip);
                using(var archive=System.IO.Compression.ZipFile.OpenRead(zip))Need(archive.GetEntry("breadcrumbs.txt")!=null && archive.GetEntry("private-file")==null,"Wrong breadcrumb export contents");
                File.WriteAllText(Path.Combine(report,"breadcrumbs.txt"),new string('x',8193));bool rejected=false;
                try{CrashReports.Export(folder,name,Path.Combine(folder,"large.zip"));}catch(System.IO.InvalidDataException){rejected=true;}
                Need(rejected && !File.Exists(Path.Combine(folder,"large.zip")),"Oversized breadcrumb file escaped export cap");
            });
            Console.WriteLine(passed+" shared launcher UI groups passed.");return 0;
        }finally{Directory.Delete(root,true);}
    }
}
