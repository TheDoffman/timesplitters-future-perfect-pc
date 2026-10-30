using System;
using System.IO;
using System.Threading.Tasks;
using Avalonia.Controls;
using Avalonia.Platform.Storage;
namespace TimeSplitters.Launcher
{
    public sealed class NativeMaintenance
    {
        readonly MaintenancePage repair=new MaintenancePage(false,true),diagnostics=new MaintenancePage(true,true);
        readonly MaintenanceArea area;
        public Control View { get { return area.View; } }
        readonly NativeWindow owner;readonly string root;VerificationResult game,launcher;bool running;
        public NativeMaintenance(string root,NativeWindow owner)
        {
            this.root=root;this.owner=owner;area=new MaintenanceArea(repair,diagnostics);
            area.Selected+=value=>{if(value)Run(Refresh);};
            repair.VerifyGame.Click+=(s,e)=>Run(()=>Verify(false));repair.VerifyLauncher.Click+=(s,e)=>Run(()=>Verify(true));
            repair.RepairGame.Click+=(s,e)=>Run(()=>Repair(false));repair.RepairLauncher.Click+=(s,e)=>Run(()=>Repair(true));
            repair.OpenFiles.Click+=(s,e)=>NativeHost.Open(root);
            foreach(var page in new[]{repair,diagnostics}){
                page.Cancel.Click+=(s,e)=>owner.CancelMaintenance();page.ViewLogs.Click+=(s,e)=>Open(".launcher");
                page.Retry.Click+=(s,e)=>{if(page.RetryOperation!=null)Run(page.RetryOperation);};
            }
            diagnostics.Refresh.Click+=(s,e)=>Run(Refresh);
            diagnostics.Copy.Click+=(s,e)=>Run(async()=>{if(owner.Clipboard!=null)await owner.Clipboard.SetTextAsync(diagnostics.Report.Text);});
            diagnostics.ReportBug.Click+=(s,e)=>Run(()=>Issue(false));diagnostics.RequestEnhancement.Click+=(s,e)=>Run(()=>Issue(true));
            diagnostics.OpenCrash.Click+=(s,e)=>Run(()=>{string latest=CrashReports.Latest(root);Core.Require(latest!=null,"No completed game report found.");NativeHost.Open(CrashReports.Folder(root,latest));return Task.CompletedTask;});
            diagnostics.ExportCrash.Click+=(s,e)=>Run(Export);Update(false);
        }
        public void Select(bool value){area.Tabs.SelectedIndex=value?1:0;if(value)Run(Refresh);}
        void Update(bool busy){area.SetBusy(busy);repair.SetVerification(game,launcher);repair.SetBusy(busy,owner.GameRunning||LauncherPlatform.Checkout(root));diagnostics.SetBusy(busy,owner.GameRunning);}
        void Status(string value){repair.Status(value);diagnostics.Status(value);}
        void Open(string relative){string path=Core.PathIn(root,relative);if(Directory.Exists(path))NativeHost.Open(path);else Status("This folder has not been created yet.");}
        async void Run(Func<Task> action)
        {
            if(running)return;running=true;
            var page=area.Tabs.SelectedIndex==1?diagnostics:repair;page.RetryOperation=action;
            try{page.ClearRecovery();Update(true);await action();}
            catch(Exception e){page.ShowRecovery(RecoveryInfo.From(e,false));}
            finally{running=false;Update(false);}
        }
        async Task Verify(bool self){await owner.Maintain(async token=>{var result=await Task.Run(()=>self?InstallationMaintenance.Verify(root,true,null,token):NativeHost.Verify(root,token));if(self)launcher=result;else game=result;Status(result.Summary);});}
        async Task Repair(bool self){if(await owner.Confirm("Repair "+(self?"launcher":"game")+"?","Restore files from the exact installed release. Saves, settings and the previous game rollback are preserved.")){await owner.Maintain(token=>owner.Repair(self,token));Status("Repair completed.");}}
        Task Refresh(){diagnostics.Report.Text=InstallationMaintenance.Report(root,game,launcher,NativeHost.Graphics());Status("Diagnostics ready. Nothing was uploaded.");return Task.CompletedTask;}
        async Task Issue(bool enhancement){await Refresh();var draft=DiagnosticIssueDraft.Create(enhancement,diagnostics.Report.Text);if(owner.Clipboard!=null)await owner.Clipboard.SetTextAsync(draft.ClipboardText);NativeHost.Open(draft.Url);}
        async Task Export(){string latest=CrashReports.Latest(root);Core.Require(latest!=null,"No completed game report found.");var file=await owner.StorageProvider.SaveFilePickerAsync(new FilePickerSaveOptions{Title="Save game report — review before sharing",SuggestedFileName=latest+".zip",FileTypeChoices=new[]{new FilePickerFileType("ZIP archive"){Patterns=new[]{"*.zip"}}}});string path=file?.TryGetLocalPath();if(path==null)return;CrashReports.Export(root,latest,path);NativeHost.Open(Path.GetDirectoryName(path));Status("Game report exported locally. Nothing was uploaded.");}
    }
}
