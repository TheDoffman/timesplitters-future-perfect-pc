using System;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;

namespace TimeSplitters.Launcher
{
    // Windows presentation adapter. All commands stay in LauncherWindow's
    // existing validated install, update, launch and recovery workflows.
    public sealed partial class LauncherWindow
    {
        internal LauncherAppearance Appearance;
        internal Release[] AppearanceCatalog=new Release[0];
        internal bool AppearanceOffline;
        internal string AppearancePrimary="PlayButton";
        internal GameChannelPickerPage AppearancePicker {get{return channelPicker;}}
        internal string AppearanceRoot {get{return root;}}
        internal SettingsPage AppearanceSettings {get{return settings;}}
        internal MaintenanceArea AppearanceMaintenance {get{return maintenanceArea;}}
        internal bool AppearanceGameInstalled {get{return gameInstalled;}}
        internal bool AppearanceDataReady {get{return validData;}}
        internal async Task AppearanceSetup(string action){
            if(busy||checking||AppearanceRunning)return;
            if(action=="install"){if(Button("UpdateButton").IsEnabled)await Update();else await Check();}
            else if(action=="iso"||action=="folder")await Import(action=="folder");
            else if(action=="play")await Play();
        }
        internal bool AppearanceBusy {get{return busy;}}
        internal bool AppearanceChecking {get{return checking;}}
        internal bool AppearanceRunning {get{return gameMonitoring || Core.GameRunning(root);}}
        internal void AppearanceRefreshPlayAvailability(bool running){Button("PlayButton").IsEnabled=!busy && gameInstalled && validData && !running;}
        internal bool AppearanceTransfer {get{return updateStop!=null;}}
        internal string AppearanceCommand {get{return updateStop!=null?"PauseDownloadButton":AppearancePrimary;}}
        internal Button AppearanceButton {get{return Button(AppearanceCommand);}}
        internal Release AppearanceInstalled {get{string tag=installed.Length>0?installed:gameInstalled?"Unversioned":"Not installed";string channel=installed.Length>0?GameChannels.Installed(root):SelectedGameChannel;return new Release {Tag=tag,Channel=channel,ChannelName=GameChannels.Name(channel)};}}
        internal string AppearancePage {
            get {
                if(Find<ContentControl>("SettingsHost").Visibility==Visibility.Visible)return "Game settings";
                if(Find<ContentControl>("SavesHost").Visibility==Visibility.Visible)return "Saves";
                if(Find<ContentControl>("NewsHost").Visibility==Visibility.Visible)return "What's new";
                if(Find<ContentControl>("VersionsHost").Visibility==Visibility.Visible)return "Versions";
                if(Find<ContentControl>("MaintenanceHost").Visibility==Visibility.Visible && maintenanceArea!=null)return maintenanceArea.Tabs.SelectedIndex==2?"Console":maintenanceArea.Tabs.SelectedIndex==1?"Diagnostics":"Repair";
                return "Home";
            }
        }
        internal bool AppearanceCanNavigate(string page){return page=="Game settings"?Button("SettingsNav").IsEnabled:page=="Saves"?Button("SavesNav").IsEnabled:page=="Versions"?Button("GameVersionsButton").IsEnabled:page=="Repair"||page=="Diagnostics"||page=="Console"?Button("RepairNav").IsEnabled:(!busy || gameMonitoring);}
        internal void AppearanceNavigate(string page){
            if(!AppearanceCanNavigate(page)){SetStatus("Finish the current operation before opening "+page+".");return;}
            if(page=="Home"||page=="Installation")ShowHome();else if(page=="Game settings")ShowSettings();else if(page=="Saves")ShowSaves();else if(page=="Versions")ShowGameVersions();else if(page=="What's new")ShowNews();else if(page=="Console")ShowConsole();else ShowMaintenance(page=="Diagnostics");
        }
        internal async Task AppearanceActivate(){
            if(!AppearanceButton.IsEnabled || AppearanceRunning)return;
            string action=AppearanceCommand;
            if(action=="PauseDownloadButton"){if(updateStop!=null)updateStop.Cancel();return;}
            if(action=="PlayButton")await Play();else if(action=="UpdateButton")await Update();else if(action=="ImportButton")await Import(false);else if(action=="RetryButton"){if(retryOperation!=null)await retryOperation();}else if(action=="OpenDataButton")OpenFolder("Data");
        }
        internal async Task AppearanceSwitch(Release target){
            if(target==null || busy || checking || AppearanceRunning)return;
            // The compact picker confirms the exact target; DownloadGame keeps
            // ownership of validation, preservation, transfer and updater handoff.
            await DownloadGame(target,true);
        }
        internal Task AppearanceCheck(){return Check();}
    }
}
