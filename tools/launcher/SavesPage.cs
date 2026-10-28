using System;
using System.IO;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
#if MACOS
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Layout;
#else
using System.Windows;
using System.Windows.Controls;
#endif

namespace TimeSplitters.Launcher
{
    public sealed class SavesPage
    {
        public readonly Grid View=new Grid();
        public readonly Button Backup, Restore, Refresh, OpenSaves, OpenBackups, Cancel;
        public readonly ComboBox Choices=new ComboBox {Name="SaveBackupChoices"};
        public event Action<bool> BusyChanged;
        public event Action<string> OpenFolder;
        readonly string root;readonly bool preview,installationLockHeld;
        readonly StackPanel body=new StackPanel();readonly TextBlock status,summary,details;
        readonly Border confirmation;CancellationTokenSource stop;
        public SavesPage(string root,bool preview,bool installationLockHeld=false)
        {
            this.root=root;this.preview=preview;this.installationLockHeld=installationLockHeld;View.Margin=new Thickness(30,24,30,18);View.Background=PageTheme.Brush("#0B1420");Ui.Theme(View);
            var dock=new DockPanel();View.Children.Add(dock);
            var footer=new DockPanel {Margin=new Thickness(0,14,0,0)};DockPanel.SetDock(footer,Dock.Bottom);dock.Children.Add(footer);
            Cancel=Button("Cancel operation","CancelSaveOperation");DockPanel.SetDock(Cancel,Dock.Right);footer.Children.Add(Cancel);Ui.Show(Cancel,false);
            status=PageTheme.Text("Automatic backups are created before game updates and restores.",12);status.Name="SaveManagerStatus";footer.Children.Add(status);
            var scroll=new ScrollViewer {VerticalScrollBarVisibility=ScrollBarVisibility.Auto,HorizontalScrollBarVisibility=ScrollBarVisibility.Disabled,Content=body};dock.Children.Add(scroll);
            body.Children.Add(PageTheme.Header("SAVE MANAGEMENT","Saves & backups","Protect your progress before updates or testing a different game version."));
            var overview=PageTheme.Section(body,"Your saves","Backups stay on this computer and include the game version and date.");
            summary=PageTheme.Text("",14);overview.Children.Add(summary);
            var actions=new WrapPanel {Margin=new Thickness(0,12,0,0)};overview.Children.Add(actions);
            Backup=Button("Back up now","CreateSaveBackup");PageTheme.Primary(Backup);actions.Children.Add(Backup);
            OpenSaves=Button("Open saves folder","OpenSaveFolder");actions.Children.Add(OpenSaves);
            OpenBackups=Button("Open backups folder","OpenSaveBackups");actions.Children.Add(OpenBackups);
            var restore=PageTheme.Section(body,"Restore a backup","Close the game first. Your current saves are backed up before replacement.");
            restore.Children.Add(Choices);details=PageTheme.Text("Select a restore point.",12,"#B4C7D8");details.Margin=new Thickness(0,10,0,12);restore.Children.Add(details);
            var restoreActions=new WrapPanel();restore.Children.Add(restoreActions);
            Restore=Button("Restore selected backup","RestoreSaveBackup");restoreActions.Children.Add(Restore);
            Refresh=Button("Refresh list","RefreshSaveBackups");restoreActions.Children.Add(Refresh);
            var storage=PageTheme.Section(body,"Backup storage","Empty regions of the game's virtual disks are omitted from the archive.");
            storage.Children.Add(PageTheme.Text("Backups are kept until you remove them. Their logical size may be much larger than the space used. Save compatibility can vary between game versions.",12));
            confirmation=new Border {Background=PageTheme.Brush("#122233"),BorderBrush=PageTheme.Brush("#86CDFF"),BorderThickness=new Thickness(1),CornerRadius=new CornerRadius(7),Padding=new Thickness(28)};
            View.Children.Add(confirmation);Ui.Show(confirmation,false);
            Choices.SelectionChanged+=(s,e)=>Select();Refresh.Click+=(s,e)=>Reload();
            Backup.Click+=async(s,e)=>await Run(async token=> {
                var progress=Reporter();var item=await Task.Run(()=>SaveBackups.Create(root,"Manual",progress,token,installationLockHeld),token);
                return item==null?"No saves found to back up yet.":"Backup created. Your saves are unchanged.";
            });
            Restore.Click+=(s,e)=>ConfirmRestore();Cancel.Click+=(s,e)=>{if(stop!=null){stop.Cancel();Cancel.IsEnabled=false;}};
            OpenSaves.Click+=(s,e)=>Open(LauncherPlatform.Saves(root));OpenBackups.Click+=(s,e)=>Open(SaveBackups.Folder);Reload();
        }
        static Button Button(string text,string name) { return new Button {Content=text,Name=name,Margin=new Thickness(0,0,10,0)}; }
        void Open(string path) { if(preview){Report("Preview only. No folders were opened.");return;}if(OpenFolder!=null)OpenFolder(path); }
        public void Report(string text) { status.Text=text; }
        IProgress<string> Reporter() {var current=stop;return new Progress<string>(text=>{if(current!=null && stop==current)Report(text);});}
        public void Reload()
        {
            if(stop!=null)return;
            try {
                string id=(Choices.SelectedItem as SaveBackup)==null?null:((SaveBackup)Choices.SelectedItem).Id;
                var items=preview?new SaveBackup[0]:SaveBackups.List(root,Report);Choices.Items.Clear();foreach(var item in items)Choices.Items.Add(item);
                Choices.SelectedItem=items.FirstOrDefault(i=>i.Id==id)??items.FirstOrDefault();
                summary.Text=items.Length==0?"No backups yet. Create your first backup before changing game versions.":items.Length+" restore point(s) · "+(items.Sum(i=>i.StoredBytes)/1048576.0).ToString("0.0")+" MiB stored";
                Select();
            }catch(Exception ex){Report("Backup list: "+ex.Message);Restore.IsEnabled=false;}
        }
        void Select()
        {
            var item=Choices.SelectedItem as SaveBackup;Restore.IsEnabled=item!=null;
            details.Text=item==null?"No restore point selected.":item.Files+" file(s) · "+(item.StoredBytes/1048576.0).ToString("0.0")+" MiB archive · "+(item.LogicalBytes/1073741824.0).ToString("0.00")+" GiB logical saves";
        }
        void ConfirmRestore()
        {
            var item=Choices.SelectedItem as SaveBackup;if(item==null || stop!=null)return;
            var prompt=new StackPanel {VerticalAlignment=VerticalAlignment.Center,MaxWidth=560};confirmation.Child=prompt;
            prompt.Children.Add(PageTheme.Text("Restore these saves?",24));
            var text=PageTheme.Text(item+"\n\nThis replaces the entire save folder. A backup of your current saves is created first. Game files, settings and your ISO are kept.",14);text.Margin=new Thickness(0,16,0,20);prompt.Children.Add(text);
            var buttons=new WrapPanel();prompt.Children.Add(buttons);Button no=Button("Keep current saves","CancelSaveRestore"),yes=Button("Restore backup","ConfirmSaveRestore");PageTheme.Primary(yes);buttons.Children.Add(no);buttons.Children.Add(yes);
            body.IsEnabled=false;Ui.Show(confirmation,true);
            no.Click+=(s,e)=>{Ui.Show(confirmation,false);body.IsEnabled=true;};
            yes.Click+=async(s,e)=>{Ui.Show(confirmation,false);body.IsEnabled=true;await Run(async token=>{
                var progress=Reporter();await Task.Run(()=>SaveBackups.Restore(root,item.Id,progress,token,null,installationLockHeld),token);return "Saves restored. Your previous saves, if any, were backed up first.";
            });};
        }
        async Task Run(Func<CancellationToken,Task<string>> action)
        {
            if(preview){Report("Preview only. No save files were changed.");return;}if(stop!=null)return;
            stop=new CancellationTokenSource();body.IsEnabled=false;Ui.Show(Cancel,true);Cancel.IsEnabled=true;if(BusyChanged!=null)BusyChanged(true);
            try { Report("Preparing save operation…");Report(await action(stop.Token)); }
            catch(OperationCanceledException){Report("Cancelled. Your current saves were kept.");}
            catch(Exception ex){Report("Save operation: "+ex.Message);}
            finally {stop.Dispose();stop=null;body.IsEnabled=true;Ui.Show(Cancel,false);Reload();if(BusyChanged!=null)BusyChanged(false);}
        }
    }
}
